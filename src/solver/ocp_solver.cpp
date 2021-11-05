#include "robotoc/solver/ocp_solver.hpp"

#include <stdexcept>
#include <cassert>


namespace robotoc {

OCPSolver::OCPSolver(const Robot& robot, 
                     const std::shared_ptr<ContactSequence>& contact_sequence,
                     const std::shared_ptr<CostFunction>& cost, 
                     const std::shared_ptr<Constraints>& constraints, 
                     const double T, const int N, const int nthreads)
  : robots_(nthreads, robot),
    contact_sequence_(contact_sequence),
    dms_(N, contact_sequence->maxNumEachEvents(), nthreads),
    sto_(),
    riccati_recursion_(robot, N, contact_sequence->maxNumEachEvents(), nthreads),
    sto_reg_(STORegularization::defaultSTORegularization()),
    line_search_(robot, N, contact_sequence->maxNumEachEvents(), nthreads),
    ocp_(robot, cost, constraints, T, N, contact_sequence->maxNumEachEvents()),
    riccati_factorization_(robot, N, contact_sequence->maxNumEachEvents()),
    kkt_matrix_(robot, N, contact_sequence->maxNumEachEvents()),
    kkt_residual_(robot, N, contact_sequence->maxNumEachEvents()),
    s_(robot, N, contact_sequence->maxNumEachEvents()),
    d_(robot, N, contact_sequence->maxNumEachEvents()),
    kkt_error_(0) {
  try {
    if (T <= 0) {
      throw std::out_of_range("invalid value: T must be positive!");
    }
    if (N <= 0) {
      throw std::out_of_range("invalid value: N must be positive!");
    }
    if (nthreads <= 0) {
      throw std::out_of_range("invalid value: nthreads must be positive!");
    }
  }
  catch(const std::exception& e) {
    std::cerr << e.what() << '\n';
    std::exit(EXIT_FAILURE);
  }
  for (auto& e : s_.data)    { robot.normalizeConfiguration(e.q); }
  for (auto& e : s_.impulse) { robot.normalizeConfiguration(e.q); }
  for (auto& e : s_.aux)     { robot.normalizeConfiguration(e.q); }
  for (auto& e : s_.lift)    { robot.normalizeConfiguration(e.q); }
}


OCPSolver::OCPSolver(const Robot& robot, 
                     const std::shared_ptr<ContactSequence>& contact_sequence,
                     const std::shared_ptr<CostFunction>& cost, 
                     const std::shared_ptr<Constraints>& constraints, 
                     const std::shared_ptr<STOCostFunction>& sto_cost, 
                     const std::shared_ptr<STOConstraints>& sto_constraints, 
                     const double T, const int N, const int nthreads)
  : robots_(nthreads, robot),
    contact_sequence_(contact_sequence),
    dms_(N, contact_sequence->maxNumEachEvents(), nthreads),
    sto_(sto_cost, sto_constraints, contact_sequence->maxNumEachEvents()),
    riccati_recursion_(robot, N, contact_sequence->maxNumEachEvents(), nthreads),
    sto_reg_(STORegularization::defaultSTORegularization()),
    line_search_(robot, N, contact_sequence->maxNumEachEvents(), nthreads),
    ocp_(robot, cost, constraints, T, N, contact_sequence->maxNumEachEvents()),
    riccati_factorization_(robot, N, contact_sequence->maxNumEachEvents()),
    kkt_matrix_(robot, N, contact_sequence->maxNumEachEvents()),
    kkt_residual_(robot, N, contact_sequence->maxNumEachEvents()),
    s_(robot, N, contact_sequence->maxNumEachEvents()),
    d_(robot, N, contact_sequence->maxNumEachEvents()),
    kkt_error_(0) {
  try {
    if (T <= 0) {
      throw std::out_of_range("invalid value: T must be positive!");
    }
    if (N <= 0) {
      throw std::out_of_range("invalid value: N must be positive!");
    }
    if (nthreads <= 0) {
      throw std::out_of_range("invalid value: nthreads must be positive!");
    }
  }
  catch(const std::exception& e) {
    std::cerr << e.what() << '\n';
    std::exit(EXIT_FAILURE);
  }
  for (auto& e : s_.data)    { robot.normalizeConfiguration(e.q); }
  for (auto& e : s_.impulse) { robot.normalizeConfiguration(e.q); }
  for (auto& e : s_.aux)     { robot.normalizeConfiguration(e.q); }
  for (auto& e : s_.lift)    { robot.normalizeConfiguration(e.q); }
}


OCPSolver::OCPSolver() {
}


OCPSolver::~OCPSolver() {
}


void OCPSolver::setDiscretizationMethod(
    const DiscretizationMethod discretization_method) {
  ocp_.setDiscretizationMethod(discretization_method);
}


void OCPSolver::meshRefinement(const double t) {
  ocp_.meshRefinement(contact_sequence_, t);
  if (ocp_.discrete().discretizationMethod() == DiscretizationMethod::PhaseBased) {
    discretizeSolution();
    dms_.initConstraints(ocp_, robots_, contact_sequence_, s_);
    sto_.initConstraints(ocp_);
  }
}


void OCPSolver::initConstraints(const double t) {
  ocp_.discretize(contact_sequence_, t);
  discretizeSolution();
  dms_.initConstraints(ocp_, robots_, contact_sequence_, s_);
  sto_.initConstraints(ocp_);
}


void OCPSolver::updateSolution(const double t, const Eigen::VectorXd& q, 
                               const Eigen::VectorXd& v, 
                               const bool line_search) {
  assert(q.size() == robots_[0].dimq());
  assert(v.size() == robots_[0].dimv());
  ocp_.discretize(contact_sequence_, t);
  discretizeSolution();
  dms_.computeKKTSystem(ocp_, robots_, contact_sequence_, q, v, s_, 
                        kkt_matrix_, kkt_residual_);
  sto_.computeKKTSystem(ocp_, kkt_matrix_, kkt_residual_);
  kkt_error_ = dms_.KKTError(ocp_, kkt_residual_) + sto_.KKTError();
  sto_reg_.applyRegularization(ocp_, kkt_error_, kkt_matrix_);
  riccati_recursion_.backwardRiccatiRecursion(ocp_, kkt_matrix_, kkt_residual_, 
                                              riccati_factorization_);
  dms_.computeInitialStateDirection(ocp_, robots_, q, v, s_, d_);
  riccati_recursion_.forwardRiccatiRecursion(ocp_, kkt_matrix_, kkt_residual_, d_);
  riccati_recursion_.computeDirection(ocp_, contact_sequence_, 
                                      riccati_factorization_, d_);
  double primal_step_size = riccati_recursion_.maxPrimalStepSize();
  const double dual_step_size = riccati_recursion_.maxDualStepSize();
  if (line_search) {
    const double max_primal_step_size = primal_step_size;
    primal_step_size = line_search_.computeStepSize(ocp_, robots_, 
                                                    contact_sequence_, 
                                                    q, v, s_, d_, 
                                                    max_primal_step_size);
  }
  dms_.integrateSolution(ocp_, robots_, primal_step_size, dual_step_size, 
                         kkt_matrix_, d_, s_);
  sto_.integrateSolution(ocp_, contact_sequence_, primal_step_size, 
                         dual_step_size, d_);
} 


const SplitSolution& OCPSolver::getSolution(const int stage) const {
  assert(stage >= 0);
  assert(stage <= ocp_.discrete().N());
  return s_[stage];
}


std::vector<Eigen::VectorXd> OCPSolver::getSolution(
    const std::string& name, const std::string& option) const {
  std::vector<Eigen::VectorXd> sol;
  if (name == "q") {
    for (int i=0; i<=ocp_.discrete().N(); ++i) {
      sol.push_back(s_[i].q);
      if (ocp_.discrete().isTimeStageBeforeImpulse(i)) {
        const int impulse_index = ocp_.discrete().impulseIndexAfterTimeStage(i);
        sol.push_back(s_.aux[impulse_index].q);
      }
      else if (ocp_.discrete().isTimeStageBeforeLift(i)) {
        const int lift_index = ocp_.discrete().liftIndexAfterTimeStage(i);
        sol.push_back(s_.lift[lift_index].q);
      }
    }
  }
  else if (name == "v") {
    for (int i=0; i<=ocp_.discrete().N(); ++i) {
      sol.push_back(s_[i].v);
      if (ocp_.discrete().isTimeStageBeforeImpulse(i)) {
        const int impulse_index = ocp_.discrete().impulseIndexAfterTimeStage(i);
        sol.push_back(s_.aux[impulse_index].v);
      }
      else if (ocp_.discrete().isTimeStageBeforeLift(i)) {
        const int lift_index = ocp_.discrete().liftIndexAfterTimeStage(i);
        sol.push_back(s_.lift[lift_index].v);
      }
    }
  }
  else if (name == "a") {
    for (int i=0; i<ocp_.discrete().N(); ++i) {
      sol.push_back(s_[i].a);
      if (ocp_.discrete().isTimeStageBeforeImpulse(i)) {
        const int impulse_index = ocp_.discrete().impulseIndexAfterTimeStage(i);
        sol.push_back(s_.aux[impulse_index].a);
      }
      else if (ocp_.discrete().isTimeStageBeforeLift(i)) {
        const int lift_index = ocp_.discrete().liftIndexAfterTimeStage(i);
        sol.push_back(s_.lift[lift_index].a);
      }
    }
  }
  else if (name == "f" && option == "WORLD") {
    Robot robot = robots_[0];
    for (int i=0; i<ocp_.discrete().N(); ++i) {
      Eigen::VectorXd f(Eigen::VectorXd::Zero(robot.max_dimf()));
      robot.updateFrameKinematics(s_[i].q);
      for (int j=0; j<robot.maxPointContacts(); ++j) {
        if (s_[i].isContactActive(j)) {
          const int contact_frame = robot.contactFrames()[j];
          robot.transformFromLocalToWorld(contact_frame, s_[i].f[j],
                                          f.template segment<3>(3*j));
        }
      }
      sol.push_back(f);
      if (ocp_.discrete().isTimeStageBeforeImpulse(i)) {
        const int impulse_index = ocp_.discrete().impulseIndexAfterTimeStage(i);
        Eigen::VectorXd f(Eigen::VectorXd::Zero(robot.max_dimf()));
        robot.updateFrameKinematics(s_.aux[impulse_index].q);
        for (int j=0; j<robot.maxPointContacts(); ++j) {
          if (s_.aux[impulse_index].isContactActive(j)) {
            const int contact_frame = robot.contactFrames()[j];
            robot.transformFromLocalToWorld(contact_frame, s_.aux[impulse_index].f[j],
                                            f.template segment<3>(3*j));
          }
        }
        sol.push_back(f);
      }
      else if (ocp_.discrete().isTimeStageBeforeLift(i)) {
        const int lift_index = ocp_.discrete().liftIndexAfterTimeStage(i);
        Eigen::VectorXd f(Eigen::VectorXd::Zero(robot.max_dimf()));
        robot.updateFrameKinematics(s_.lift[lift_index].q);
        for (int j=0; j<robot.maxPointContacts(); ++j) {
          if (s_.lift[lift_index].isContactActive(j)) {
            const int contact_frame = robot.contactFrames()[j];
            robot.transformFromLocalToWorld(contact_frame, s_.lift[lift_index].f[j],
                                            f.template segment<3>(3*j));
          }
        }
        sol.push_back(f);
      }
    }
  }
  else if (name == "f") {
    Robot robot = robots_[0];
    for (int i=0; i<ocp_.discrete().N(); ++i) {
      Eigen::VectorXd f(Eigen::VectorXd::Zero(robot.max_dimf()));
      for (int j=0; j<robot.maxPointContacts(); ++j) {
        if (s_[i].isContactActive(j)) {
          f.template segment<3>(3*j) = s_[i].f[j];
        }
      }
      sol.push_back(f);
      if (ocp_.discrete().isTimeStageBeforeImpulse(i)) {
        const int impulse_index = ocp_.discrete().impulseIndexAfterTimeStage(i);
        Eigen::VectorXd f(Eigen::VectorXd::Zero(robot.max_dimf()));
        for (int j=0; j<robot.maxPointContacts(); ++j) {
          if (s_.aux[impulse_index].isContactActive(j)) {
            f.template segment<3>(3*j) = s_.aux[impulse_index].f[j];
          }
        }
        sol.push_back(f);
      }
      else if (ocp_.discrete().isTimeStageBeforeLift(i)) {
        const int lift_index = ocp_.discrete().liftIndexAfterTimeStage(i);
        Eigen::VectorXd f(Eigen::VectorXd::Zero(robot.max_dimf()));
        for (int j=0; j<robot.maxPointContacts(); ++j) {
          if (s_.lift[lift_index].isContactActive(j)) {
            f.template segment<3>(3*j) = s_.lift[lift_index].f[j];
          }
        }
        sol.push_back(f);
      }
    }
  }
  else if (name == "u") {
    for (int i=0; i<ocp_.discrete().N(); ++i) {
      sol.push_back(s_[i].u);
      if (ocp_.discrete().isTimeStageBeforeImpulse(i)) {
        const int impulse_index = ocp_.discrete().impulseIndexAfterTimeStage(i);
        sol.push_back(s_.aux[impulse_index].u);
      }
      else if (ocp_.discrete().isTimeStageBeforeLift(i)) {
        const int lift_index = ocp_.discrete().liftIndexAfterTimeStage(i);
        sol.push_back(s_.lift[lift_index].u);
      }
    }
  }
  else if (name == "ts") {
    const int num_events = ocp_.discrete().N_impulse()+ocp_.discrete().N_lift();
    int impulse_index = 0;
    int lift_index = 0;
    Eigen::VectorXd ts(num_events);
    for (int event_index=0; event_index<num_events; ++event_index) {
      if (ocp_.discrete().eventType(event_index) == DiscreteEventType::Impulse) {
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


void OCPSolver::getStateFeedbackGain(const int time_stage, Eigen::MatrixXd& Kq, 
                                     Eigen::MatrixXd& Kv) const {
  assert(time_stage >= 0);
  assert(time_stage < ocp_.discrete().N());
  assert(Kq.rows() == robots_[0].dimv());
  assert(Kq.cols() == robots_[0].dimv());
  assert(Kv.rows() == robots_[0].dimv());
  assert(Kv.cols() == robots_[0].dimv());
  riccati_recursion_.getStateFeedbackGain(time_stage, Kq, Kv);
}


void OCPSolver::setSolution(const std::string& name, 
                            const Eigen::VectorXd& value) {
  try {
    if (name == "q") {
      for (auto& e : s_.data)    { e.q = value; }
      for (auto& e : s_.impulse) { e.q = value; }
      for (auto& e : s_.aux)     { e.q = value; }
      for (auto& e : s_.lift)    { e.q = value; }
    }
    else if (name == "v") {
      for (auto& e : s_.data)    { e.v = value; }
      for (auto& e : s_.impulse) { e.v = value; }
      for (auto& e : s_.aux)     { e.v = value; }
      for (auto& e : s_.lift)    { e.v = value; }
    }
    else if (name == "a") {
      for (auto& e : s_.data)    { e.a  = value; }
      for (auto& e : s_.impulse) { e.dv = value; }
      for (auto& e : s_.aux)     { e.a  = value; }
      for (auto& e : s_.lift)    { e.a  = value; }
    }
    else if (name == "f") {
      for (auto& e : s_.data) { 
        for (auto& ef : e.f) { ef = value; } 
        e.set_f_stack(); 
      }
      for (auto& e : s_.aux) { 
        for (auto& ef : e.f) { ef = value; } 
        e.set_f_stack(); 
      }
      for (auto& e : s_.lift) { 
        for (auto& ef : e.f) { ef = value; } 
        e.set_f_stack(); 
      }
    }
    else if (name == "lmd") {
      for (auto& e : s_.impulse) { 
        for (auto& ef : e.f) { ef = value; } 
        e.set_f_stack(); 
      }
    }
    else if (name == "u") {
      for (auto& e : s_.data)    { e.u = value; }
      for (auto& e : s_.aux)     { e.u = value; }
      for (auto& e : s_.lift)    { e.u = value; }
    }
    else {
      throw std::invalid_argument("invalid arugment: name must be q, v, a, f, or u!");
    }
  }
  catch(const std::exception& e) {
    std::cerr << e.what() << '\n';
    std::exit(EXIT_FAILURE);
  }
}


void OCPSolver::extrapolateSolutionLastPhase(const double t) {
  const int num_discrete_events = contact_sequence_->numDiscreteEvents();
  if (num_discrete_events > 0) {
    ocp_.discretize(contact_sequence_, t);
    int time_stage_after_last_event;
    if (contact_sequence_->eventType(num_discrete_events-1) 
          == DiscreteEventType::Impulse) {
      time_stage_after_last_event 
          = ocp_.discrete().timeStageAfterImpulse(ocp_.discrete().N_impulse()-1);
    }
    else {
      time_stage_after_last_event 
          = ocp_.discrete().timeStageAfterLift(ocp_.discrete().N_lift()-1);
    }
    for (int i=time_stage_after_last_event; i<=ocp_.discrete().N(); ++i) {
      s_[i].copyPrimal(s_[time_stage_after_last_event-1]);
      s_[i].copyDual(s_[time_stage_after_last_event-1]);
      ocp_[i].initConstraints(ocp_[time_stage_after_last_event-1]);
    }
  }
}


void OCPSolver::extrapolateSolutionInitialPhase(const double t) {
  const int num_discrete_events = contact_sequence_->numDiscreteEvents();
  if (num_discrete_events > 0) {
    ocp_.discretize(contact_sequence_, t);
    int time_stage_before_initial_event;
    if (contact_sequence_->eventType(0) == DiscreteEventType::Impulse) {
      time_stage_before_initial_event 
          = ocp_.discrete().timeStageBeforeImpulse(0);
    }
    else {
      time_stage_before_initial_event 
          = ocp_.discrete().timeStageBeforeLift(0);
    }
    for (int i=0; i<=time_stage_before_initial_event; ++i) {
      s_[i].copyPrimal(s_[time_stage_before_initial_event+1]);
      s_[i].copyDual(s_[time_stage_before_initial_event+1]);
      ocp_[i].initConstraints(ocp_[time_stage_before_initial_event+1]);
    }
  }
}


void OCPSolver::clearLineSearchFilter() {
  line_search_.clearFilter();
}


double OCPSolver::KKTError() {
  return kkt_error_;
}


double OCPSolver::cost() const {
  return dms_.totalCost(ocp_);
}


void OCPSolver::computeKKTResidual(const double t, const Eigen::VectorXd& q, 
                                   const Eigen::VectorXd& v) {
  ocp_.discretize(contact_sequence_, t);
  discretizeSolution();
  dms_.computeKKTResidual(ocp_, robots_, contact_sequence_, q, v, s_, 
                          kkt_matrix_, kkt_residual_);
  sto_.computeKKTResidual(ocp_, kkt_residual_);
  kkt_error_ = dms_.KKTError(ocp_, kkt_residual_) + sto_.KKTError();
}


bool OCPSolver::isCurrentSolutionFeasible(const bool verbose) {
  // ocp_.discretize(contact_sequence_, t);
  // discretizeSolution();
  return dms_.isFeasible(ocp_, robots_, contact_sequence_, s_);
}


const HybridOCPDiscretization& OCPSolver::getOCPDiscretization() const {
  return ocp_.discrete();
}


void OCPSolver::discretizeSolution() {
  for (int i=0; i<=ocp_.discrete().N(); ++i) {
    s_[i].setContactStatus(
        contact_sequence_->contactStatus(ocp_.discrete().contactPhase(i)));
    s_[i].set_f_stack();
    s_[i].setImpulseStatus();
  }
  for (int i=0; i<ocp_.discrete().N_lift(); ++i) {
    s_.lift[i].setContactStatus(
        contact_sequence_->contactStatus(
            ocp_.discrete().contactPhaseAfterLift(i)));
    s_.lift[i].set_f_stack();
    s_.lift[i].setImpulseStatus();
  }
  for (int i=0; i<ocp_.discrete().N_impulse(); ++i) {
    s_.impulse[i].setImpulseStatus(contact_sequence_->impulseStatus(i));
    s_.impulse[i].set_f_stack();
    s_.aux[i].setContactStatus(
        contact_sequence_->contactStatus(
            ocp_.discrete().contactPhaseAfterImpulse(i)));
    s_.aux[i].set_f_stack();
    const int time_stage_before_impulse 
        = ocp_.discrete().timeStageBeforeImpulse(i);
    if (time_stage_before_impulse-1 >= 0) {
      s_[time_stage_before_impulse-1].setImpulseStatus(
          contact_sequence_->impulseStatus(i));
    }
  }
}


void OCPSolver::setSTORegularization(const STORegularization& sto_reg) {
  sto_reg_ = sto_reg;
}


void OCPSolver::setLineSearchSettings(const LineSearchSettings& settings) {
  line_search_.set(settings);
}


void OCPSolver::disp(std::ostream& os) const {
  os << contact_sequence_ << std::endl;
  os << ocp_.discrete() << std::endl;
}


std::ostream& operator<<(std::ostream& os, const OCPSolver& ocp_solver) {
  ocp_solver.disp(os);
  return os;
}

} // namespace robotoc