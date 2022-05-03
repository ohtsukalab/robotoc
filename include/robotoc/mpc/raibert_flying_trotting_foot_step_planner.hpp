#ifndef ROBOTOC_RAIBERT_FLYING_TROTTING_FOOT_STEP_PLANNER_HPP_
#define ROBOTOC_RAIBERT_FLYING_TROTTING_FOOT_STEP_PLANNER_HPP_

#include <vector>
#include <iostream>

#include "Eigen/Core"
#include "Eigen/Geometry"

#include "robotoc/robot/robot.hpp"
#include "robotoc/robot/contact_status.hpp"
#include "robotoc/robot/se3.hpp"
#include "robotoc/utils/aligned_vector.hpp"
#include "robotoc/mpc/foot_step_planner_base.hpp"
#include "robotoc/mpc/raibert_heuristic.hpp"


namespace robotoc {

///
/// @class RaibertFlyingTrottingFootStepPlanner
/// @brief Foot step planner for the flying trotting gait of quadrupeds with
/// the Raibert heuristic. 
///
class RaibertFlyingTrottingFootStepPlanner final : public FootStepPlannerBase {
public:
  ///
  /// @brief Constructs the planner.
  /// @param[in] quadruped_robot Quadruped robot model. 
  ///
  RaibertFlyingTrottingFootStepPlanner(const Robot& quadruped_robot);

  ///
  /// @brief Default constructor. 
  ///
  RaibertFlyingTrottingFootStepPlanner();

  ///
  /// @brief Destructor. 
  ///
  ~RaibertFlyingTrottingFootStepPlanner();

  ///
  /// @brief Default copy constructor. 
  ///
  RaibertFlyingTrottingFootStepPlanner(
      const RaibertFlyingTrottingFootStepPlanner&) = default;

  ///
  /// @brief Default copy assign operator. 
  ///
  RaibertFlyingTrottingFootStepPlanner& operator=(
      const RaibertFlyingTrottingFootStepPlanner&) = default;

  ///
  /// @brief Default move constructor. 
  ///
  RaibertFlyingTrottingFootStepPlanner(
      RaibertFlyingTrottingFootStepPlanner&&) noexcept = default;

  ///
  /// @brief Default move assign operator. 
  ///
  RaibertFlyingTrottingFootStepPlanner& operator=(
      RaibertFlyingTrottingFootStepPlanner&&) noexcept = default;

  ///
  /// @brief Sets the gait pattern. 
  /// @param[in] v_com_cmd Command of the COM velocity. 
  /// @param[in] yaw_rate_cmd Command of the yaw-rate of the body. 
  /// @param[in] t_swing Duration of swing. 
  /// @param[in] t_stance Duration of stance. 
  /// @param[in] gain The feedback gain of the v_com_cmd. 
  ///
  void setGaitPattern(const Eigen::Vector3d& v_com_cmd, 
                      const double yaw_rate_cmd, const double t_swing, 
                      const double t_stance, const double gain);

  void init(const Eigen::VectorXd& q) override;

  bool plan(const Eigen::VectorXd& q, const Eigen::VectorXd& v, 
            const ContactStatus& contact_status, 
            const int planning_steps) override;

  ///
  /// @brief This is invalid in FlyingTrottingFootStepPlanner. 
  ///
  const aligned_vector<SE3>& contactPlacement(const int step) const override;

  ///
  /// @brief This is invalid in CrawlingFootStepPlanner. 
  ///
  const aligned_vector<aligned_vector<SE3>>& contactPlacement() const override;

  const std::vector<Eigen::Vector3d>& contactPosition(const int step) const override;

  const std::vector<std::vector<Eigen::Vector3d>>& contactPosition() const override;

  const Eigen::Vector3d& com(const int step) const override;

  const std::vector<Eigen::Vector3d>& com() const override;

  const Eigen::Matrix3d& R(const int step) const override;

  const std::vector<Eigen::Matrix3d>& R() const override;

  void disp(std::ostream& os) const;

  friend std::ostream& operator<<(std::ostream& os, 
                                  const RaibertFlyingTrottingFootStepPlanner& planner);

  friend std::ostream& operator<<(std::ostream& os, 
                                  const std::shared_ptr<RaibertFlyingTrottingFootStepPlanner>& planner);

  EIGEN_MAKE_ALIGNED_OPERATOR_NEW

private:
  Robot robot_;
  RaibertHeuristic raibert_heuristic_;
  int LF_foot_id_, LH_foot_id_, RF_foot_id_, RH_foot_id_, current_step_;
  aligned_vector<aligned_vector<SE3>> contact_placement_ref_;
  std::vector<std::vector<Eigen::Vector3d>> contact_position_ref_;
  std::vector<Eigen::Vector3d> com_ref_, com_to_contact_position_local_;
  std::vector<Eigen::Matrix3d> R_;
  Eigen::Vector3d v_com_cmd_, step_length_;
  Eigen::Matrix3d R_yaw_;
  double yaw_rate_cmd_;

};

} // namespace robotoc 

#endif // ROBOTOC_RAIBERT_FLYING_TROTTING_FOOT_STEP_PLANNER_HPP_