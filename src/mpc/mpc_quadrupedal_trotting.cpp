#include "robotoc/mpc/mpc_quadrupedal_trotting.hpp"

#include <stdexcept>
#include <iostream>
#include <cassert>
#include <cmath>
#include <algorithm>


namespace robotoc {

MPCQuadrupedalTrotting::MPCQuadrupedalTrotting(const OCP& ocp, 
                                               const int nthreads)
  : foot_step_planner_(std::make_shared<TrottingFootStepPlanner>(ocp.robot())),
    contact_sequence_(std::make_shared<robotoc::ContactSequence>(
        ocp.robot(), ocp.maxNumEachDiscreteEvents())),
    ocp_solver_(ocp, contact_sequence_, SolverOptions::defaultOptions(), nthreads), 
    solver_options_(SolverOptions::defaultOptions()),
    cs_standing_(ocp.robot().createContactStatus()),
    cs_lfrh_(ocp.robot().createContactStatus()),
    cs_rflh_(ocp.robot().createContactStatus()),
    vcom_(Eigen::Vector3d::Zero()),
    step_length_(Eigen::Vector3d::Zero()),
    step_height_(0),
    swing_time_(0),
    initial_lift_time_(0),
    T_(ocp.T()),
    dt_(ocp.T()/ocp.N()),
    dtm_(ocp.T()/ocp.N()),
    ts_last_(0),
    eps_(std::sqrt(std::numeric_limits<double>::epsilon())),
    N_(ocp.N()),
    current_step_(0),
    predict_step_(0) {
  cs_standing_.activateContacts({0, 1, 2, 3});
  cs_lfrh_.activateContacts({0, 3});
  cs_rflh_.activateContacts({1, 2});
}


MPCQuadrupedalTrotting::MPCQuadrupedalTrotting() {
}


MPCQuadrupedalTrotting::~MPCQuadrupedalTrotting() {
}


void MPCQuadrupedalTrotting::setGaitPattern(const Eigen::Vector3d& vcom, 
                                            const double yaw_rate,
                                            const double swing_time,
                                            const double initial_lift_time) {
  try {
    if (swing_time <= 0) {
      throw std::out_of_range("invalid value: swing_time must be positive!");
    }
    if (initial_lift_time <= 0) {
      throw std::out_of_range("invalid value: initial_lift_time must be positive!");
    }
  }
  catch(const std::exception& e) {
    std::cerr << e.what() << '\n';
    std::exit(EXIT_FAILURE);
  }
  vcom_ = vcom;
  step_length_ = vcom * swing_time;
  swing_time_ = swing_time;
  initial_lift_time_ = initial_lift_time;
  foot_step_planner_->setGaitPattern(step_length_, (swing_time*yaw_rate));
}


void MPCQuadrupedalTrotting::init(const double t, const Eigen::VectorXd& q, 
                                  const Eigen::VectorXd& v, 
                                  const SolverOptions& solver_options) {
  try {
    if (t >= initial_lift_time_) {
      throw std::out_of_range(
          "invalid value: t must be less than" + std::to_string(initial_lift_time_) + "!");
    }
  }
  catch(const std::exception& e) {
    std::cerr << e.what() << '\n';
    std::exit(EXIT_FAILURE);
  }
  current_step_ = 0;
  predict_step_ = 0;
  contact_sequence_->initContactSequence(cs_standing_);
  bool add_step = addStep(t);
  while (add_step) {
    add_step = addStep(t);
  }
  foot_step_planner_->init(q);
  resetContactPlacements(q);
  ocp_solver_.setSolution("q", q);
  ocp_solver_.setSolution("v", v);
  ocp_solver_.setSolverOptions(solver_options);
  ocp_solver_.solve(t, q, v, true);
  ts_last_ = initial_lift_time_;
}


void MPCQuadrupedalTrotting::setSolverOptions(
    const SolverOptions& solver_options) {
  ocp_solver_.setSolverOptions(solver_options);
}


void MPCQuadrupedalTrotting::updateSolution(const double t, const double dt,
                                            const Eigen::VectorXd& q, 
                                            const Eigen::VectorXd& v) {
  assert(dt > 0);
  const bool add_step = addStep(t);
  const auto ts = contact_sequence_->eventTimes();
  bool remove_step = false;
  if (!ts.empty()) {
    if (ts.front()+eps_ < t+dt) {
      ts_last_ = ts.front();
      ocp_solver_.extrapolateSolutionInitialPhase(t);
      contact_sequence_->pop_front();
      remove_step = true;
      ++current_step_;
    }
  }
  resetContactPlacements(q);
  ocp_solver_.solve(t, q, v, true);
}


const Eigen::VectorXd& MPCQuadrupedalTrotting::getInitialControlInput() const {
  return ocp_solver_.getSolution(0).u;
}


double MPCQuadrupedalTrotting::KKTError(const double t, const Eigen::VectorXd& q, 
                                        const Eigen::VectorXd& v) {
  return ocp_solver_.KKTError(t, q, v);
}


double MPCQuadrupedalTrotting::KKTError() const {
  return ocp_solver_.KKTError();
}


bool MPCQuadrupedalTrotting::addStep(const double t) {
  if (predict_step_ == 0) {
    if (initial_lift_time_ < t+T_-dtm_) {
      contact_sequence_->push_back(cs_lfrh_, initial_lift_time_);
      ++predict_step_;
      return true;
    }
  }
  else {
    double tt = ts_last_ + swing_time_;
    const auto ts = contact_sequence_->eventTimes();
    if (!ts.empty()) {
      tt = ts.back() + swing_time_;
    }
    if (tt < t+T_-dtm_) {
      if (predict_step_%2 != 0) {
        contact_sequence_->push_back(cs_rflh_, tt);
      }
      else {
        contact_sequence_->push_back(cs_lfrh_, tt);
      }
      ++predict_step_;
      return true;
    }
  }
  return false;
}


void MPCQuadrupedalTrotting::resetContactPlacements(const Eigen::VectorXd& q) {
  const bool success = foot_step_planner_->plan(q, contact_sequence_->contactStatus(0),
                                                contact_sequence_->numContactPhases()+1);
  for (int phase=0; phase<contact_sequence_->numContactPhases(); ++phase) {
    contact_sequence_->setContactPlacements(phase, 
                                            foot_step_planner_->contactPosition(phase+1));
  }
  // std::cout << contact_sequence_->contactStatus(0) << std::endl;
  // std::cout << foot_step_planner_ << std::endl;
  // const double first_rate 
  //     = std::max(((t_-initial_lift_time_-(current_step_-1)*swing_time_) / swing_time_), 0.0);
  // const double last_rate 
  //     = std::max(((initial_lift_time_+predict_step_*swing_time_-t_-T_) / swing_time_), 0.0);
  // std::cout << "first_rate: " << first_rate << std::endl;
  // std::cout << "last_rate: " << last_rate << std::endl;
  // std::cout << "com_first: " << foot_step_planner_->com(0).transpose() << std::endl;
  // std::cout << "com_last: " << foot_step_planner_->com(contact_sequence_->numContactPhases()+1).transpose() << std::endl;
  // com_ref_->setCoMRef(contact_sequence_, com_prev_, com_, first_rate, last_rate);
}

} // namespace robotoc 