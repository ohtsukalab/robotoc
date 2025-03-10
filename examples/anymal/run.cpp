#include <string>
#include <memory>

#include "Eigen/Core"

#include "robotoc/solver/ocp_solver.hpp"
#include "robotoc/ocp/ocp.hpp"
#include "robotoc/robot/robot.hpp"
#include "robotoc/planner/contact_sequence.hpp"
#include "robotoc/cost/cost_function.hpp"
#include "robotoc/cost/configuration_space_cost.hpp"
#include "robotoc/constraints/constraints.hpp"
#include "robotoc/constraints/joint_position_lower_limit.hpp"
#include "robotoc/constraints/joint_position_upper_limit.hpp"
#include "robotoc/constraints/joint_velocity_lower_limit.hpp"
#include "robotoc/constraints/joint_velocity_upper_limit.hpp"
#include "robotoc/constraints/joint_torques_lower_limit.hpp"
#include "robotoc/constraints/joint_torques_upper_limit.hpp"
#include "robotoc/constraints/friction_cone.hpp"
#include "robotoc/constraints/impact_friction_cone.hpp"
#include "robotoc/solver/solver_options.hpp"

#include "robotoc/utils/ocp_benchmarker.hpp"

 
class ConfigurationSpaceRef final : public robotoc::ConfigurationSpaceRefBase {
public:
  ConfigurationSpaceRef(const double t0, 
                        const double period_init1, 
                        const double period_init2, 
                        const double period,
                        const double period_final, 
                        const int steps,
                        const Eigen::VectorXd& q0, const double v_ref) 
    : ConfigurationSpaceRefBase(),
      t0_(t0),
      period_init1_(period_init1),
      period_init2_(period_init2),
      period_(period),
      period_final_(period_final), 
      steps_(steps),
      tf_(t0+period_init1+period_init2+steps*period+period_final),
      q0_(q0),
      qf_(q0),
      v_ref_(v_ref),
      v_ref_init1_(0.25*v_ref*period/period_init1),
      v_ref_init2_(0.5*v_ref*period/period_init2),
      v_ref_final_(0.75*v_ref*period/period_final) {
    qf_.coeffRef(0) += period_init1 * v_ref_init1_;
    qf_.coeffRef(0) += period_init2 * v_ref_init2_;
    qf_.coeffRef(0) += steps * period * v_ref;
    qf_.coeffRef(0) += period_final * v_ref_final_;
  }

  ~ConfigurationSpaceRef() {}

  void updateRef(const robotoc::Robot& robot, 
                 const robotoc::GridInfo& grid_info, 
                 Eigen::VectorXd& q_ref) const override {
    if (grid_info.t < t0_) {
      q_ref = q0_;
    }
    else if (grid_info.t < t0_ + period_init1_) {
      q_ref = q0_;
      q_ref.coeffRef(0) += (grid_info.t-t0_) * v_ref_init1_;
    }
    else if (grid_info.t < t0_ + period_init1_ + period_init2_) {
      q_ref = q0_;
      q_ref.coeffRef(0) += period_init1_ * v_ref_init1_;
      q_ref.coeffRef(0) += (grid_info.t-t0_-period_init1_) * v_ref_init2_;
    }
    else if (grid_info.t < tf_-period_final_) {
      q_ref = q0_;
      q_ref.coeffRef(0) += period_init1_ * v_ref_init1_;
      q_ref.coeffRef(0) += period_init2_ * v_ref_init2_;
      q_ref.coeffRef(0) += (grid_info.t-t0_-period_init1_-period_init2_) * v_ref_;
    }
    else if (grid_info.t < tf_) {
      q_ref = q0_;
      q_ref.coeffRef(0) += period_init1_ * v_ref_init1_;
      q_ref.coeffRef(0) += period_init2_ * v_ref_init2_;
      q_ref.coeffRef(0) += steps_ * period_ * v_ref_;
      q_ref.coeffRef(0) += (grid_info.t-t0_-period_init1_-period_init2_-steps_*period_) * v_ref_final_;
    }
    else {
      q_ref = qf_;
    }
  }

  bool isActive(const robotoc::GridInfo& grid_info) const override {
    return true;
  }

private:
  Eigen::VectorXd q0_, qf_;
  double t0_, period_init1_, period_init2_, period_, period_final_, tf_, 
         v_ref_, v_ref_init1_, v_ref_init2_, v_ref_final_;
  int steps_;
};


int main(int argc, char *argv[]) {
  robotoc::RobotModelInfo model_info;
  model_info.urdf_path = "../anymal_b_simple_description/urdf/anymal.urdf";
  model_info.base_joint_type = robotoc::BaseJointType::FloatingBase;
  const double baumgarte_time_step = 0.04;
  model_info.point_contacts = {robotoc::ContactModelInfo("LF_FOOT", baumgarte_time_step),
                               robotoc::ContactModelInfo("LH_FOOT", baumgarte_time_step),
                               robotoc::ContactModelInfo("RF_FOOT", baumgarte_time_step),
                               robotoc::ContactModelInfo("RH_FOOT", baumgarte_time_step)};
  robotoc::Robot robot(model_info);

  const double stride = 0.45;
  const double additive_stride_hip = 0.2;
  const double t_start = 1.0;

  const double t_front_swing = 0.135;
  const double t_front_hip_swing = 0.05;
  const double t_hip_swing = 0.165;
  const double t_period = t_front_swing + t_front_hip_swing + t_hip_swing;
  const int steps = 10;

  // Create the cost function
  auto cost = std::make_shared<robotoc::CostFunction>();
  Eigen::VectorXd v_weight(Eigen::VectorXd::Zero(robot.dimv()));
  v_weight << 0.01, 0.01, 0.01, 0.1, 0.1, 0.1, 
              0.1, 0.1, 0.1,
              0.1, 0.1, 0.1,
              0.1, 0.1, 0.1,
              0.1, 0.1, 0.1;
  Eigen::VectorXd a_weight(Eigen::VectorXd::Constant(robot.dimv(), 0.001));

  auto config_cost = std::make_shared<robotoc::ConfigurationSpaceCost>(robot);
  config_cost->set_v_weight(v_weight);
  config_cost->set_v_weight_terminal(v_weight);
  config_cost->set_v_weight_impact(v_weight);
  config_cost->set_a_weight(a_weight);
  config_cost->set_dv_weight_impact(a_weight);
  cost->add("config_cost", config_cost);

  Eigen::VectorXd q_standing(Eigen::VectorXd::Zero(robot.dimq()));
  q_standing << -3, 0, 0.4792, 0, 0, 0, 1, 
                -0.1,  0.7, -1.0, 
                -0.1, -0.7,  1.0, 
                 0.1,  0.7, -1.0, 
                 0.1, -0.7,  1.0;
  Eigen::VectorXd q_weight(Eigen::VectorXd::Zero(robot.dimv()));
  q_weight << 100, 100, 100, 100, 100, 100, 
              1, 1, 1,
              1, 1, 1,
              1, 1, 1,
              1, 1, 1;
  const double v_ref = stride / t_period;
  auto config_ref = std::make_shared<ConfigurationSpaceRef>(t_start, 0.255, 0.34, t_period, 0.5, steps,
                                                            q_standing, v_ref);
  auto time_varying_config_cost = std::make_shared<robotoc::ConfigurationSpaceCost>(robot, config_ref);
  time_varying_config_cost->set_q_weight(q_weight);
  time_varying_config_cost->set_q_weight_terminal(q_weight);
  time_varying_config_cost->set_q_weight_impact(q_weight);
  cost->add("time_varying_config_cost", time_varying_config_cost);

  // Create the constraints
  const double barrier_param = 1.0e-03;
  const double fraction_to_boundary_rule = 0.995;
  auto constraints           = std::make_shared<robotoc::Constraints>(barrier_param, fraction_to_boundary_rule);
  auto joint_position_lower  = std::make_shared<robotoc::JointPositionLowerLimit>(robot);
  auto joint_position_upper  = std::make_shared<robotoc::JointPositionUpperLimit>(robot);
  auto joint_velocity_lower  = std::make_shared<robotoc::JointVelocityLowerLimit>(robot);
  auto joint_velocity_upper  = std::make_shared<robotoc::JointVelocityUpperLimit>(robot);
  auto joint_torques_lower   = std::make_shared<robotoc::JointTorquesLowerLimit>(robot);
  auto joint_torques_upper   = std::make_shared<robotoc::JointTorquesUpperLimit>(robot);
  auto friction_cone         = std::make_shared<robotoc::FrictionCone>(robot);
  auto impact_friction_cone = std::make_shared<robotoc::ImpactFrictionCone>(robot);
  constraints->add("joint_position_lower", joint_position_lower);
  constraints->add("joint_position_upper", joint_position_upper);
  constraints->add("joint_velocity_lower", joint_velocity_lower);
  constraints->add("joint_velocity_upper", joint_velocity_upper);
  constraints->add("joint_torques_lower", joint_torques_lower);
  constraints->add("joint_torques_upper", joint_torques_upper);
  constraints->add("friction_cone", friction_cone);
  constraints->push_back(impact_friction_cone);

  // Create the contact sequence
  auto contact_sequence = std::make_shared<robotoc::ContactSequence>(robot);
  const double mu = 0.7;
  const std::unordered_map<std::string, double> friction_coefficients = {{"LF_FOOT", mu}, 
                                                                         {"LH_FOOT", mu}, 
                                                                         {"RF_FOOT", mu}, 
                                                                         {"RH_FOOT", mu}};

  robot.updateFrameKinematics(q_standing);
  std::unordered_map<std::string, Eigen::Vector3d> contact_positions 
      = {{"LF_FOOT", robot.framePosition("LF_FOOT")}, 
         {"LH_FOOT", robot.framePosition("LH_FOOT")}, 
         {"RF_FOOT", robot.framePosition("RF_FOOT")}, 
         {"RH_FOOT", robot.framePosition("RH_FOOT")}};
  auto contact_status_standing = robot.createContactStatus();
  contact_status_standing.activateContacts(std::vector<std::string>({"LF_FOOT", "LH_FOOT", "RF_FOOT", "RH_FOOT"}));
  auto contact_status_front_swing = robot.createContactStatus();
  contact_status_front_swing.activateContacts(std::vector<std::string>({"LH_FOOT", "RH_FOOT"}));
  auto contact_status_hip_swing = robot.createContactStatus();
  contact_status_hip_swing.activateContacts(std::vector<std::string>({"LF_FOOT", "RF_FOOT"}));
  auto contact_status_front_hip_swing = robot.createContactStatus();

  contact_status_standing.setFrictionCoefficients(friction_coefficients);
  contact_status_front_swing.setFrictionCoefficients(friction_coefficients);
  contact_status_hip_swing.setFrictionCoefficients(friction_coefficients);

  contact_status_standing.setContactPlacements(contact_positions);
  contact_sequence->init(contact_status_standing);

  const double t_initial_front_swing = 0.125;
  const double t_initial_front_hip_swing = 0.05;
  const double t_initial_hip_swing = 0.125;
  const double t_initial = t_initial_front_swing + t_initial_front_hip_swing + t_initial_hip_swing;
  const double t_initial_front_swing2 = 0.135;
  const double t_initial_front_hip_swing2 = 0.055;
  const double t_initial_hip_swing2 = 0.15;
  const double t_initial2 = t_initial_front_swing2 + t_initial_front_hip_swing2 + t_initial_hip_swing2;

  contact_status_front_swing.setContactPlacements(contact_positions);
  contact_sequence->push_back(contact_status_front_swing, t_start);
  contact_status_front_hip_swing.setContactPlacements(contact_positions);
  contact_sequence->push_back(contact_status_front_hip_swing, 
                              t_start+t_initial_front_swing);

  contact_positions["LF_FOOT"].coeffRef(0) += 0.25 * stride;
  contact_positions["RF_FOOT"].coeffRef(0) += 0.25 * stride;
  contact_positions["LH_FOOT"].coeffRef(0) += 0.25 * stride + 0.5 * additive_stride_hip;
  contact_positions["RH_FOOT"].coeffRef(0) += 0.25 * stride + 0.5 * additive_stride_hip;

  contact_status_hip_swing.setContactPlacements(contact_positions);
  contact_sequence->push_back(contact_status_hip_swing, 
                              t_start+t_initial_front_swing+t_initial_front_hip_swing);

  contact_status_front_swing.setContactPlacements(contact_positions);
  contact_sequence->push_back(contact_status_front_swing, t_start+t_initial);
  contact_status_front_hip_swing.setContactPlacements(contact_positions);
  contact_sequence->push_back(contact_status_front_hip_swing, 
                              t_start+t_initial+t_initial_front_swing2);

  contact_positions["LF_FOOT"].coeffRef(0) += 0.5 * stride;
  contact_positions["RF_FOOT"].coeffRef(0) += 0.5 * stride;
  contact_positions["LH_FOOT"].coeffRef(0) += 0.5 * stride + 0.5 * additive_stride_hip;
  contact_positions["RH_FOOT"].coeffRef(0) += 0.5 * stride + 0.5 * additive_stride_hip;

  contact_status_hip_swing.setContactPlacements(contact_positions);
  contact_sequence->push_back(contact_status_hip_swing, 
                              t_start+t_initial+t_initial_front_swing2+t_initial_front_hip_swing2);
  const double t_end_init = t_start+t_initial+t_initial2;

  for (int i=0; i<steps; ++i) {
    contact_status_front_swing.setContactPlacements(contact_positions);
    contact_sequence->push_back(contact_status_front_swing, t_end_init+i*t_period);
    contact_sequence->push_back(contact_status_front_hip_swing, 
                                t_end_init+i*t_period+t_front_swing);
    contact_positions["LF_FOOT"].coeffRef(0) += stride;
    contact_positions["LH_FOOT"].coeffRef(0) += stride;
    contact_positions["RF_FOOT"].coeffRef(0) += stride;
    contact_positions["RH_FOOT"].coeffRef(0) += stride;
    contact_status_hip_swing.setContactPlacements(contact_positions);
    contact_sequence->push_back(contact_status_hip_swing, 
                                t_end_init+i*t_period+t_front_swing+t_front_hip_swing);
  }

  contact_status_front_swing.setContactPlacements(contact_positions);
  contact_sequence->push_back(contact_status_front_swing, t_end_init+steps*t_period);

  // For the last step
  const double t_end_front_swing = 0.15;
  const double t_end_front_hip_swing = 0.05;
  const double t_end_hip_swing = 0.15;
  const double t_end = t_end_front_swing + t_end_front_hip_swing + t_end_hip_swing;

  contact_sequence->push_back(contact_status_front_hip_swing, 
                              t_end_init+steps*t_period+t_end_front_swing);

  contact_positions["LF_FOOT"].coeffRef(0) += 0.75 * stride;
  contact_positions["LH_FOOT"].coeffRef(0) += 0.75 * stride;
  contact_positions["RF_FOOT"].coeffRef(0) += 0.75 * stride - additive_stride_hip;
  contact_positions["RH_FOOT"].coeffRef(0) += 0.75 * stride - additive_stride_hip;
  contact_status_hip_swing.setContactPlacements(contact_positions);
  contact_sequence->push_back(contact_status_hip_swing, 
                              t_end_init+steps*t_period+t_end_front_swing+t_end_front_hip_swing);
  contact_status_standing.setContactPlacements(contact_positions);
  contact_sequence->push_back(contact_status_standing, t_end_init+steps*t_period+t_end);

  // you can check the contact sequence via
  // std::cout << contact_sequence << std::endl;

  // Create the OCP solver.
  const double T = 7; 
  const int N = 240;
  robotoc::OCP ocp(robot, cost, constraints, contact_sequence, T, N);
  auto solver_options = robotoc::SolverOptions();
  solver_options.nthreads = 4;
  robotoc::OCPSolver ocp_solver(ocp, solver_options);

  // Initial time and initial state
  const double t = 0;
  const Eigen::VectorXd q(q_standing);
  const Eigen::VectorXd v(Eigen::VectorXd::Zero(robot.dimv()));

  // Solves the OCP.
  ocp_solver.discretize(t);
  ocp_solver.setSolution("q", q);
  ocp_solver.setSolution("v", v);
  Eigen::Vector3d f_init;
  f_init << 0, 0, 0.25*robot.totalWeight();
  ocp_solver.setSolution("f", f_init);
  ocp_solver.setSolution("lmd", f_init);
  ocp_solver.initConstraints();
  std::cout << "Initial KKT error: " << ocp_solver.KKTError(t, q, v) << std::endl;
  ocp_solver.solve(t, q, v);
  std::cout << "KKT error after convergence: " << ocp_solver.KKTError(t, q, v) << std::endl;
  std::cout << ocp_solver.getSolverStatistics() << std::endl;

  // const int num_iteration = 10000;
  // robotoc::benchmark::CPUTime(ocp_solver, t, q, v, num_iteration);

  return 0;
}