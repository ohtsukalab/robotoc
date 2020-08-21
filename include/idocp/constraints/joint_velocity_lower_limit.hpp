#ifndef IDOCP_JOINT_VELOCITY_LOWER_LIMIT_HPP_
#define IDOCP_JOINT_VELOCITY_LOWER_LIMIT_HPP_

#include "Eigen/Core"

#include "idocp/robot/robot.hpp"
#include "idocp/ocp/split_solution.hpp"
#include "idocp/ocp/split_direction.hpp"
#include "idocp/constraints/constraint_component_base.hpp"
#include "idocp/constraints/constraint_component_data.hpp"
#include "idocp/ocp/kkt_residual.hpp"
#include "idocp/ocp/kkt_matrix.hpp"


namespace idocp {

class JointVelocityLowerLimit final : public ConstraintComponentBase {
public:
  JointVelocityLowerLimit(const Robot& robot, const double barrier=1.0e-08,
                          const double fraction_to_boundary_rate=0.995);

  JointVelocityLowerLimit();

  ~JointVelocityLowerLimit();

  // Use default copy constructor.
  JointVelocityLowerLimit(const JointVelocityLowerLimit&) = default;

  // Use default copy coperator.
  JointVelocityLowerLimit& operator=(const JointVelocityLowerLimit&) = default;

  // Use default move constructor.
  JointVelocityLowerLimit(JointVelocityLowerLimit&&) noexcept = default;

  // Use default move assign coperator.
  JointVelocityLowerLimit& operator=(JointVelocityLowerLimit&&) noexcept 
      = default;

  bool isFeasible(const Robot& robot, ConstraintComponentData& data, 
                  const SplitSolution& s) const override;

  void setSlackAndDual(const Robot& robot, ConstraintComponentData& data, 
                       const double dtau, const SplitSolution& s) const override;

  void augmentDualResidual(const Robot& robot, ConstraintComponentData& data, 
                           const double dtau, 
                           KKTResidual& kkt_residual) const override;

  void augmentDualResidual(const Robot& robot, ConstraintComponentData& data, 
                           const double dtau, 
                           Eigen::VectorXd& lu) const override {}

  void condenseSlackAndDual(const Robot& robot, ConstraintComponentData& data, 
                            const double dtau, const SplitSolution& s,
                            KKTMatrix& kkt_matrix,
                            KKTResidual& kkt_residual) const override;

  void condenseSlackAndDual(const Robot& robot, ConstraintComponentData& data, 
                            const double dtau, const Eigen::VectorXd& u,
                            Eigen::MatrixXd& Quu, 
                            Eigen::VectorXd& lu) const override {}

  void computeSlackAndDualDirection(const Robot& robot, 
                                    ConstraintComponentData& data, 
                                    const double dtau, 
                                    const SplitDirection& d) const override; 

  double residualL1Nrom(const Robot& robot, ConstraintComponentData& data, 
                        const double dtau, 
                        const SplitSolution& s) const override;

  double squaredKKTErrorNorm(const Robot& robot, ConstraintComponentData& data, 
                             const double dtau, 
                             const SplitSolution& s) const override;
  
  int dimc() const override;

private:
  int dimc_, dim_passive_;
  Eigen::VectorXd vmin_;

};

} // namespace idocp

#endif // IDOCP_JOINT_VELOCITY_LOWER_LIMIT_HPP_