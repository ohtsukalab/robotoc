#include "robotoc/solver/ocp_solver.hpp"

#include <stdexcept>
#include <cassert>
#include <algorithm>


namespace robotoc {

auto createOCPDef = [](const OCP& ocp) {
  OCPDef def;
  auto robot = ocp.robot();
  auto cost = ocp.cost();
  auto constraints = ocp.constraints();
  auto contact_sequence = ocp.contact_sequence();
  def.robot = robot;
  def.cost = cost;
  def.constraints = constraints;
  def.contact_sequence = contact_sequence;
  def.T = ocp.T();
  def.N = ocp.N();
  def.num_reserved_discrete_events = 3*ocp.reservedNumDiscreteEvents();
  return def;
};


OCPSolver::OCPSolver(const OCP& ocp, 
                     const SolverOptions& solver_options, const int nthreads)
  : robots_(nthreads, ocp.robot()),
    contact_sequence_(ocp.contact_sequence()),
    dms_(createOCPDef(ocp), nthreads),
    time_discretization_(ocp.T(), ocp.N(), ocp.reservedNumDiscreteEvents()),
    sto_(ocp),
    riccati_recursion_(ocp, nthreads, solver_options.max_dts_riccati),
    line_search_(ocp, nthreads),
    ocp_(ocp),
    riccati_factorization_(ocp.robot(), ocp.N()+3*ocp.reservedNumDiscreteEvents()+1, ocp.reservedNumDiscreteEvents()),
    kkt_matrix_(ocp.robot(), ocp.N()+3*ocp.reservedNumDiscreteEvents()+1, ocp.reservedNumDiscreteEvents()),
    kkt_residual_(ocp.robot(), ocp.N()+3*ocp.reservedNumDiscreteEvents()+1, ocp.reservedNumDiscreteEvents()),
    s_(ocp.robot(), ocp.N()+3*ocp.reservedNumDiscreteEvents()+1, ocp.reservedNumDiscreteEvents()),
    d_(ocp.robot(), ocp.N()+3*ocp.reservedNumDiscreteEvents()+1, ocp.reservedNumDiscreteEvents()),
    solution_interpolator_(ocp.robot(), ocp.N()+3*ocp.reservedNumDiscreteEvents()+1, ocp.reservedNumDiscreteEvents()),
    solver_options_(solver_options),
    solver_statistics_() {
  if (nthreads <= 0) {
    throw std::out_of_range("[OCPSolver] invalid argument: nthreads must be positive!");
  }
  for (auto& e : s_.data)    { ocp.robot().normalizeConfiguration(e.q); }
  for (auto& e : s_.impulse) { ocp.robot().normalizeConfiguration(e.q); }
  for (auto& e : s_.aux)     { ocp.robot().normalizeConfiguration(e.q); }
  for (auto& e : s_.lift)    { ocp.robot().normalizeConfiguration(e.q); }
  time_discretization_.setDiscretizationMethod(solver_options.discretization_method);
}


OCPSolver::OCPSolver() {
}


OCPSolver::~OCPSolver() {
}


void OCPSolver::setSolverOptions(const SolverOptions& solver_options) {
  solver_options_ = solver_options;
  time_discretization_.setDiscretizationMethod(solver_options.discretization_method);
  riccati_recursion_.setRegularization(solver_options.max_dts_riccati);
}


void OCPSolver::meshRefinement(const double t) {
  time_discretization_.discretizeGrid(contact_sequence_, t);
  if (solver_options_.discretization_method == DiscretizationMethod::PhaseBased) {
    std::cout << "solver_options_.discretization_method == PhaseBased" << std::endl;
    time_discretization_.discretizePhase(contact_sequence_, t);
    reserveData();
    discretizeSolution();
  }
}


void OCPSolver::initConstraints(const double t) {
  meshRefinement(t);
  dms_.initConstraints(robots_, time_discretization_, s_);
}


void OCPSolver::updateSolution(const double t, const Eigen::VectorXd& q, 
                               const Eigen::VectorXd& v) {
  assert(q.size() == robots_[0].dimq());
  assert(v.size() == robots_[0].dimv());
  dms_.evalKKT(robots_, time_discretization_, q, v, s_, kkt_matrix_, kkt_residual_);
  // sto_.computeKKTSystem(ocp_, kkt_matrix_, kkt_residual_);
  // sto_.applyRegularization(ocp_, kkt_matrix_);
  riccati_recursion_.backwardRiccatiRecursion(time_discretization_, 
                                              kkt_matrix_, kkt_residual_, 
                                              riccati_factorization_);
  dms_.computeInitialStateDirection(robots_[0], q, v, s_, d_);
  riccati_recursion_.forwardRiccatiRecursion(time_discretization_, 
                                             kkt_matrix_, kkt_residual_, 
                                             riccati_factorization_, d_);
  dms_.computeStepSizes(time_discretization_, d_);
  // sto_.computeDirection(ocp_, d_);
  // double primal_step_size = std::min(dms_.maxPrimalStepSize(), 
  //                                    sto_.maxPrimalStepSize());
  // const double dual_step_size = std::min(dms_.maxDualStepSize(),
  //                                        sto_.maxDualStepSize());
  double primal_step_size = dms_.maxPrimalStepSize();
  const double dual_step_size = dms_.maxDualStepSize();
  // if (solver_options_.enable_line_search) {
  //   const double max_primal_step_size = primal_step_size;
  //   primal_step_size = line_search_.computeStepSize(ocp_, robots_, 
  //                                                   contact_sequence_, 
  //                                                   q, v, s_, d_, 
  //                                                   max_primal_step_size);
  // }
  solver_statistics_.primal_step_size.push_back(primal_step_size);
  solver_statistics_.dual_step_size.push_back(dual_step_size);
  dms_.integrateSolution(robots_, time_discretization_, 
                         primal_step_size, dual_step_size, kkt_matrix_, d_, s_);
  // sto_.integrateSolution(ocp_, contact_sequence_, primal_step_size, 
  //                        dual_step_size, d_);
} 


void OCPSolver::solve(const double t, const Eigen::VectorXd& q, 
                      const Eigen::VectorXd& v, const bool init_solver) {
  if (q.size() != robots_[0].dimq()) {
    throw std::out_of_range("[OCPSolver] invalid argument: q.size() must be " + std::to_string(robots_[0].dimq()) + "!");
  }
  if (v.size() != robots_[0].dimv()) {
    throw std::out_of_range("[OCPSolver] invalid argument: v.size() must be " + std::to_string(robots_[0].dimv()) + "!");
  }
  if (solver_options_.enable_benchmark) {
    timer_.tick();
  }
  if (init_solver) {
    meshRefinement(t);
    if (solver_options_.enable_solution_interpolation) {
      solution_interpolator_.interpolate(robots_[0], time_discretization_, s_);
    }
    dms_.initConstraints(robots_, time_discretization_, s_);
    line_search_.clearFilter();
  }
  solver_statistics_.clear(); 
  int inner_iter = 0;
  for (int iter=0; iter<solver_options_.max_iter; ++iter, ++inner_iter) {
    if (ocp_.isSTOEnabled()) {
      if (inner_iter < solver_options_.initial_sto_reg_iter) {
        sto_.setRegularization(solver_options_.initial_sto_reg);
      }
      else {
        sto_.setRegularization(0);
      }
      solver_statistics_.ts.emplace_back(contact_sequence_->eventTimes());
    } 
    updateSolution(t, q, v);
    const double kkt_error = KKTError();
    solver_statistics_.kkt_error.push_back(kkt_error); 
    if (ocp_.isSTOEnabled() && (kkt_error < solver_options_.kkt_tol_mesh)) {
      if (time_discretization_.dt_max() > solver_options_.max_dt_mesh) {
        if (solver_options_.enable_solution_interpolation) {
          solution_interpolator_.store(time_discretization_, s_);
        }
        meshRefinement(t);
        if (solver_options_.enable_solution_interpolation) {
          solution_interpolator_.interpolate(robots_[0], time_discretization_, s_);
        }
        dms_.initConstraints(robots_, time_discretization_, s_);
        inner_iter = 0;
        solver_statistics_.mesh_refinement_iter.push_back(iter+1); 
      }
      else if (kkt_error < solver_options_.kkt_tol) {
        solver_statistics_.convergence = true;
        solver_statistics_.iter = iter+1;
        break;
      }
    }
    else if (kkt_error < solver_options_.kkt_tol) {
      solver_statistics_.convergence = true;
      solver_statistics_.iter = iter+1;
      break;
    }
  }
  if (!solver_statistics_.convergence) {
    solver_statistics_.iter = solver_options_.max_iter;
  }
  if (solver_options_.enable_solution_interpolation) {
    solution_interpolator_.store(time_discretization_, s_);
  }
  if (solver_options_.enable_benchmark) {
    timer_.tock();
    solver_statistics_.cpu_time = timer_.ms();
  }
}


const SolverStatistics& OCPSolver::getSolverStatistics() const {
  return solver_statistics_;
}


const Solution& OCPSolver::getSolution() const {
  return s_;
}


const SplitSolution& OCPSolver::getSolution(const int stage) const {
  assert(stage >= 0);
  assert(stage <= time_discretization_.N());
  return s_[stage];
}


std::vector<Eigen::VectorXd> OCPSolver::getSolution(
    const std::string& name, const std::string& option) const {
  std::vector<Eigen::VectorXd> sol;
  if (name == "q") {
    for (int i=0; i<=ocp_.timeDiscretization().N(); ++i) {
      sol.push_back(s_[i].q);
      if (ocp_.timeDiscretization().isTimeStageBeforeImpulse(i)) {
        const int impulse_index = ocp_.timeDiscretization().impulseIndexAfterTimeStage(i);
        sol.push_back(s_.aux[impulse_index].q);
      }
      else if (ocp_.timeDiscretization().isTimeStageBeforeLift(i)) {
        const int lift_index = ocp_.timeDiscretization().liftIndexAfterTimeStage(i);
        sol.push_back(s_.lift[lift_index].q);
      }
    }
  }
  else if (name == "v") {
    for (int i=0; i<=ocp_.timeDiscretization().N(); ++i) {
      sol.push_back(s_[i].v);
      if (ocp_.timeDiscretization().isTimeStageBeforeImpulse(i)) {
        const int impulse_index = ocp_.timeDiscretization().impulseIndexAfterTimeStage(i);
        sol.push_back(s_.aux[impulse_index].v);
      }
      else if (ocp_.timeDiscretization().isTimeStageBeforeLift(i)) {
        const int lift_index = ocp_.timeDiscretization().liftIndexAfterTimeStage(i);
        sol.push_back(s_.lift[lift_index].v);
      }
    }
  }
  else if (name == "a") {
    for (int i=0; i<ocp_.timeDiscretization().N(); ++i) {
      sol.push_back(s_[i].a);
      if (ocp_.timeDiscretization().isTimeStageBeforeImpulse(i)) {
        const int impulse_index = ocp_.timeDiscretization().impulseIndexAfterTimeStage(i);
        sol.push_back(s_.aux[impulse_index].a);
      }
      else if (ocp_.timeDiscretization().isTimeStageBeforeLift(i)) {
        const int lift_index = ocp_.timeDiscretization().liftIndexAfterTimeStage(i);
        sol.push_back(s_.lift[lift_index].a);
      }
    }
  }
  else if (name == "f" && option == "WORLD") {
    Robot robot = robots_[0];
    for (int i=0; i<ocp_.timeDiscretization().N(); ++i) {
      Eigen::VectorXd f(Eigen::VectorXd::Zero(robot.max_dimf()));
      robot.updateFrameKinematics(s_[i].q);
      for (int j=0; j<robot.maxNumContacts(); ++j) {
        if (s_[i].isContactActive(j)) {
          const int contact_frame = robot.contactFrames()[j];
          robot.transformFromLocalToWorld(contact_frame, s_[i].f[j].template head<3>(),
                                          f.template segment<3>(3*j));
        }
      }
      sol.push_back(f);
      if (ocp_.timeDiscretization().isTimeStageBeforeImpulse(i)) {
        const int impulse_index = ocp_.timeDiscretization().impulseIndexAfterTimeStage(i);
        Eigen::VectorXd f(Eigen::VectorXd::Zero(robot.max_dimf()));
        robot.updateFrameKinematics(s_.aux[impulse_index].q);
        for (int j=0; j<robot.maxNumContacts(); ++j) {
          if (s_.aux[impulse_index].isContactActive(j)) {
            const int contact_frame = robot.contactFrames()[j];
            robot.transformFromLocalToWorld(contact_frame, 
                                            s_.aux[impulse_index].f[j].template head<3>(),
                                            f.template segment<3>(3*j));
          }
        }
        sol.push_back(f);
      }
      else if (ocp_.timeDiscretization().isTimeStageBeforeLift(i)) {
        const int lift_index = ocp_.timeDiscretization().liftIndexAfterTimeStage(i);
        Eigen::VectorXd f(Eigen::VectorXd::Zero(robot.max_dimf()));
        robot.updateFrameKinematics(s_.lift[lift_index].q);
        for (int j=0; j<robot.maxNumContacts(); ++j) {
          if (s_.lift[lift_index].isContactActive(j)) {
            const int contact_frame = robot.contactFrames()[j];
            robot.transformFromLocalToWorld(contact_frame, 
                                            s_.lift[lift_index].f[j].template head<3>(), 
                                            f.template segment<3>(3*j));
          }
        }
        sol.push_back(f);
      }
    }
  }
  else if (name == "f") {
    Robot robot = robots_[0];
    for (int i=0; i<ocp_.timeDiscretization().N(); ++i) {
      Eigen::VectorXd f(Eigen::VectorXd::Zero(robot.max_dimf()));
      for (int j=0; j<robot.maxNumContacts(); ++j) {
        if (s_[i].isContactActive(j)) {
          f.template segment<3>(3*j) = s_[i].f[j].template head<3>();
        }
      }
      sol.push_back(f);
      if (ocp_.timeDiscretization().isTimeStageBeforeImpulse(i)) {
        const int impulse_index = ocp_.timeDiscretization().impulseIndexAfterTimeStage(i);
        Eigen::VectorXd f(Eigen::VectorXd::Zero(robot.max_dimf()));
        for (int j=0; j<robot.maxNumContacts(); ++j) {
          if (s_.aux[impulse_index].isContactActive(j)) {
            f.template segment<3>(3*j) = s_.aux[impulse_index].f[j].template head<3>();
          }
        }
        sol.push_back(f);
      }
      else if (ocp_.timeDiscretization().isTimeStageBeforeLift(i)) {
        const int lift_index = ocp_.timeDiscretization().liftIndexAfterTimeStage(i);
        Eigen::VectorXd f(Eigen::VectorXd::Zero(robot.max_dimf()));
        for (int j=0; j<robot.maxNumContacts(); ++j) {
          if (s_.lift[lift_index].isContactActive(j)) {
            f.template segment<3>(3*j) = s_.lift[lift_index].f[j].template head<3>();
          }
        }
        sol.push_back(f);
      }
    }
  }
  else if (name == "u") {
    for (int i=0; i<ocp_.timeDiscretization().N(); ++i) {
      sol.push_back(s_[i].u);
      if (ocp_.timeDiscretization().isTimeStageBeforeImpulse(i)) {
        const int impulse_index = ocp_.timeDiscretization().impulseIndexAfterTimeStage(i);
        sol.push_back(s_.aux[impulse_index].u);
      }
      else if (ocp_.timeDiscretization().isTimeStageBeforeLift(i)) {
        const int lift_index = ocp_.timeDiscretization().liftIndexAfterTimeStage(i);
        sol.push_back(s_.lift[lift_index].u);
      }
    }
  }
  else if (name == "ts") {
    const int num_events = ocp_.timeDiscretization().N_impulse()+ocp_.timeDiscretization().N_lift();
    int impulse_index = 0;
    int lift_index = 0;
    Eigen::VectorXd ts(num_events);
    for (int event_index=0; event_index<num_events; ++event_index) {
      if (ocp_.timeDiscretization().eventType(event_index) == DiscreteEventType::Impulse) {
        ts.coeffRef(event_index) = contact_sequence_->impulseTime(impulse_index);
        ++impulse_index;
      }
      else {
        ts.coeffRef(event_index) = contact_sequence_->liftTime(lift_index);
        ++lift_index;
      }
    }
    sol.push_back(ts);
  }
  return sol;
}


const hybrid_container<LQRPolicy>& OCPSolver::getLQRPolicy() const {
  return riccati_recursion_.getLQRPolicy();
}


const RiccatiFactorization& OCPSolver::getRiccatiFactorization() const {
  return riccati_factorization_;
}


void OCPSolver::setSolution(const Solution& s) {
  assert(s.data.size() == s_.data.size());
  assert(s.lift.size() == s_.lift.size());
  assert(s.aux.size() == s_.aux.size());
  assert(s.impulse.size() == s_.impulse.size());
  if (s.data.size() != s_.data.size()) {
    throw std::out_of_range(
        "[OCPSolver] invalid argument: s.data.size() must be " + std::to_string(s_.data.size()) + "!");
  }
  if (s.lift.size() != s_.lift.size()) {
    throw std::out_of_range(
        "[OCPSolver] invalid argument: s.lift.size() must be " + std::to_string(s_.lift.size()) + "!");
  }
  if (s.aux.size() != s_.aux.size()) {
    throw std::out_of_range(
        "[OCPSolver] invalid argument: s.aux.size() must be " + std::to_string(s_.aux.size()) + "!");
  }
  if (s.impulse.size() != s_.impulse.size()) {
    throw std::out_of_range(
        "[OCPSolver] invalid argument: s.impulse.size() must be " + std::to_string(s_.impulse.size()) + "!");
  }
  s_ = s;
}


void OCPSolver::setSolution(const std::string& name, 
                            const Eigen::VectorXd& value) {
  if (name == "q") {
    if (value.size() != robots_[0].dimq()) {
      throw std::out_of_range(
          "[OCPSolver] invalid argument: q.size() must be " + std::to_string(robots_[0].dimq()) + "!");
    }
    for (auto& e : s_.data)    { e.q = value; }
    for (auto& e : s_.impulse) { e.q = value; }
    for (auto& e : s_.aux)     { e.q = value; }
    for (auto& e : s_.lift)    { e.q = value; }
  }
  else if (name == "v") {
    if (value.size() != robots_[0].dimv()) {
      throw std::out_of_range(
          "[OCPSolver] invalid argument: v.size() must be " + std::to_string(robots_[0].dimv()) + "!");
    }
    for (auto& e : s_.data)    { e.v = value; }
    for (auto& e : s_.impulse) { e.v = value; }
    for (auto& e : s_.aux)     { e.v = value; }
    for (auto& e : s_.lift)    { e.v = value; }
  }
  else if (name == "a") {
    if (value.size() != robots_[0].dimv()) {
      throw std::out_of_range(
          "[OCPSolver] invalid argument: a.size() must be " + std::to_string(robots_[0].dimv()) + "!");
    }
    for (auto& e : s_.data)    { e.a  = value; }
    for (auto& e : s_.impulse) { e.dv = value; }
    for (auto& e : s_.aux)     { e.a  = value; }
    for (auto& e : s_.lift)    { e.a  = value; }
  }
  else if (name == "f") {
    if (value.size() == 6) {
      for (auto& e : s_.data) { 
        for (auto& ef : e.f) { ef = value.template head<6>(); } 
        e.set_f_stack(); 
      }
      for (auto& e : s_.aux) { 
        for (auto& ef : e.f) { ef = value.template head<6>(); } 
        e.set_f_stack(); 
      }
      for (auto& e : s_.lift) { 
        for (auto& ef : e.f) { ef = value.template head<6>(); } 
        e.set_f_stack(); 
      }
    }
    else if (value.size() == 3) {
      for (auto& e : s_.data) { 
        for (auto& ef : e.f) { ef.template head<3>() = value.template head<3>(); } 
        e.set_f_stack(); 
      }
      for (auto& e : s_.aux) { 
        for (auto& ef : e.f) { ef.template head<3>() = value.template head<3>(); } 
        e.set_f_stack(); 
      }
      for (auto& e : s_.lift) { 
        for (auto& ef : e.f) { ef.template head<3>() = value.template head<3>(); } 
        e.set_f_stack(); 
      }
    }
    else {
      throw std::out_of_range("[OCPSolver] invalid argument: f.size() must be 3 or 6!");
    }
  }
  else if (name == "lmd") {
    if (value.size() != 6) {
      for (auto& e : s_.impulse) { 
        for (auto& ef : e.f) { ef = value.template head<6>(); } 
        e.set_f_stack(); 
      }
    }
    else if (value.size() != 3) {
      for (auto& e : s_.impulse) { 
        for (auto& ef : e.f) { ef.template head<3>() = value.template head<3>(); } 
        e.set_f_stack(); 
      }
    }
    else {
      throw std::out_of_range("[OCPSolver] invalid argument: f.size() must be 3 or 6!");
    }
  }
  else if (name == "u") {
    if (value.size() != robots_[0].dimu()) {
      throw std::out_of_range(
          "[OCPSolver] invalid argument: u.size() must be " + std::to_string(robots_[0].dimu()) + "!");
    }
    for (auto& e : s_.data)    { e.u = value; }
    for (auto& e : s_.aux)     { e.u = value; }
    for (auto& e : s_.lift)    { e.u = value; }
  }
  else {
    throw std::invalid_argument("[OCPSolver] invalid arugment: name must be q, v, a, f, or u!");
  }
}


double OCPSolver::KKTError(const double t, const Eigen::VectorXd& q, 
                           const Eigen::VectorXd& v) {
  if (q.size() != robots_[0].dimq()) {
    throw std::out_of_range("[OCPSolver] invalid argument: q.size() must be " + std::to_string(robots_[0].dimq()) + "!");
  }
  if (v.size() != robots_[0].dimv()) {
    throw std::out_of_range("[OCPSolver] invalid argument: v.size() must be " + std::to_string(robots_[0].dimv()) + "!");
  }
  ocp_.discretize(t);
  reserveData();
  discretizeSolution();
  dms_.evalKKT(robots_, time_discretization_, q, v, s_, kkt_matrix_, kkt_residual_);
  return KKTError();
}


double OCPSolver::KKTError() const {
  return dms_.getEval(time_discretization_).kkt_error;
}


double OCPSolver::cost(const bool include_cost_barrier) const {
  if (include_cost_barrier) {
    const auto eval = dms_.getEval(time_discretization_);
    return eval.cost + eval.cost_barrier;
  }
  else {
    return dms_.getEval(time_discretization_).cost;
  }
}


bool OCPSolver::isCurrentSolutionFeasible(const bool verbose) {
  // ocp_.discretize(t);
  // reserveData();
  // discretizeSolution();
  return dms_.isFeasible(robots_, time_discretization_, s_);
}


const TimeDiscretization& OCPSolver::getTimeDiscretization() const {
  return time_discretization_;
}


void OCPSolver::setRobotProperties(const RobotProperties& properties) {
  for (auto& e : robots_) {
    e.setRobotProperties(properties);
  }
}


void OCPSolver::reserveData() {
  kkt_matrix_.reserve(ocp_.robot(), ocp_.reservedNumDiscreteEvents());
  kkt_residual_.reserve(ocp_.robot(), ocp_.reservedNumDiscreteEvents());
  s_.reserve(ocp_.robot(), ocp_.reservedNumDiscreteEvents());
  d_.reserve(ocp_.robot(), ocp_.reservedNumDiscreteEvents());
  riccati_factorization_.reserve(ocp_.robot(), ocp_.reservedNumDiscreteEvents());
  riccati_recursion_.reserve(ocp_);
  line_search_.reserve(ocp_);

  kkt_matrix_.data.resize(ocp_.N()+1+3*ocp_.reservedNumDiscreteEvents());
  kkt_residual_.data.resize(ocp_.N()+1+3*ocp_.reservedNumDiscreteEvents());
  s_.data.resize(ocp_.N()+1+3*ocp_.reservedNumDiscreteEvents());
  d_.data.resize(ocp_.N()+1+3*ocp_.reservedNumDiscreteEvents());
  riccati_factorization_.data.resize(ocp_.N()+1+3*ocp_.reservedNumDiscreteEvents());
}


void OCPSolver::discretizeSolution() {
  for (int i=0; i<=time_discretization_.N_grids(); ++i) {
    const auto& grid = time_discretization_.grid(i);
    if (grid.type == GridType::Intermediate || grid.type == GridType::Lift) {
      s_[i].setContactStatus(contact_sequence_->contactStatus(grid.contact_phase));
      s_[i].set_f_stack();
    }
    else if (grid.type == GridType::Impulse) {
      s_[i].setContactStatus(contact_sequence_->impulseStatus(grid.impulse_index));
      s_[i].set_f_stack();
    }
    if (grid.switching_constraint) {
      const auto& grid_next_next = time_discretization_.grid(i+2);
      s_[i].setSwitchingConstraintDimension(contact_sequence_->impulseStatus(grid_next_next.impulse_index).dimf());
    }
    else {
      s_[i].setSwitchingConstraintDimension(0);
    }
  }
}


void OCPSolver::disp(std::ostream& os) const {
  os << ocp_ << std::endl;
}


std::ostream& operator<<(std::ostream& os, const OCPSolver& ocp_solver) {
  ocp_solver.disp(os);
  return os;
}

} // namespace robotoc