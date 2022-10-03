#ifndef ROBOTOC_UNCONSTR_PARNMPC_STAGE_HPP_
#define ROBOTOC_UNCONSTR_PARNMPC_STAGE_HPP_

#include <memory>

#include "Eigen/Core"

#include "robotoc/robot/robot.hpp"
#include "robotoc/robot/contact_status.hpp"
#include "robotoc/core/split_solution.hpp"
#include "robotoc/core/split_direction.hpp"
#include "robotoc/core/split_kkt_residual.hpp"
#include "robotoc/core/split_kkt_matrix.hpp"
#include "robotoc/cost/cost_function.hpp"
#include "robotoc/cost/cost_function_data.hpp"
#include "robotoc/constraints/constraints.hpp"
#include "robotoc/constraints/constraints_data.hpp"
#include "robotoc/unconstr/unconstr_ocp_data.hpp"
#include "robotoc/ocp/grid_info.hpp"


namespace robotoc {

///
/// @class ParNMPCTerminalStage
/// @brief An optimal control problem of unconstrained rigid-body systems for 
/// ParNMPC algorithm split into the terminal stage. 
///
class ParNMPCTerminalStage {
public:
  ///
  /// @brief Constructs a split optimal control problem.
  /// @param[in] robot Robot model. 
  /// @param[in] cost Shared ptr to the cost function.
  /// @param[in] constraints Shared ptr to the constraints.
  ///
  ParNMPCTerminalStage(const Robot& robot, 
                       const std::shared_ptr<CostFunction>& cost,
                       const std::shared_ptr<Constraints>& constraints);

  ///
  /// @brief Default constructor.  
  ///
  ParNMPCTerminalStage();

  ///
  /// @brief Default destructor. 
  ///
  ~ParNMPCTerminalStage() = default;

  ///
  /// @brief Default copy constructor. 
  ///
  ParNMPCTerminalStage(const ParNMPCTerminalStage&) = default;

  ///
  /// @brief Default copy assign operator. 
  ///
  ParNMPCTerminalStage& operator=(const ParNMPCTerminalStage&) = default;

  ///
  /// @brief Default move constructor. 
  ///
  ParNMPCTerminalStage(ParNMPCTerminalStage&&) noexcept = default;

  ///
  /// @brief Default move assign operator. 
  ///
  ParNMPCTerminalStage& operator=(ParNMPCTerminalStage&&) noexcept = default;

  UnconstrOCPData createData(const Robot& robot) const;

  ///
  /// @brief Checks whether the solution is feasible under inequality constraints.
  /// @param[in, out] robot Robot model. 
  /// @param[in] grid_info Grid info of this stage.
  /// @param[in] s Split solution of this stage.
  /// @param[in, out] data Data of this stage. 
  ///
  bool isFeasible(Robot& robot, const GridInfo& grid_info,
                  const SplitSolution& s, UnconstrOCPData& data) const;

  ///
  /// @brief Initializes the constraints, i.e., set slack and dual variables. 
  /// @param[in] robot Robot model. 
  /// @param[in] time_stage Time stage.
  /// @param[in] s Split solution of this time stage.
  ///
  void initConstraints(Robot& robot, const GridInfo& grid_info, 
                       const SplitSolution& s, UnconstrOCPData& data) const;

  ///
  /// @brief Computes the stage cost and constraint violation.
  /// Used in the line search.
  /// @param[in] robot Robot model. 
  /// @param[in] grid_info Grid info. 
  /// @param[in] s Split solution of this time stage.
  /// @param[in, out] kkt_residual Split KKT residual of this time stage.
  ///
  void evalOCP(Robot& robot, const GridInfo& grid_info, 
               const Eigen::VectorXd& q_prev, const Eigen::VectorXd& v_prev, 
               const SplitSolution& s, UnconstrOCPData& data, 
               SplitKKTResidual& kkt_residual) const;

  ///
  /// @brief Computes the KKT system of this time stage, i.e., the condensed
  /// KKT matrix and KKT residual of this time stage for Newton's method.
  /// @param[in] robot Robot model. 
  /// @param[in] grid_info Grid info. 
  /// @param[in] s Split solution of this time stage.
  /// @param[in, out] kkt_matrix Split KKT matrix of this time stage.
  /// @param[in, out] kkt_residual Split KKT residual of this time stage.
  ///
  void evalKKT(Robot& robot, const GridInfo& grid_info, 
               const Eigen::VectorXd& q_prev, const Eigen::VectorXd& v_prev, 
               const SplitSolution& s, UnconstrOCPData& data,
               SplitKKTMatrix& kkt_matrix, SplitKKTResidual& kkt_residual) const;

  ///
  /// @brief Expands the primal and dual variables, i.e., computes the Newton 
  /// direction of the condensed variables of this stage.
  /// @param[in, out] d Split direction of this time stage.
  /// 
  void expandPrimalAndDual(const double dt, const SplitKKTMatrix& kkt_matrix, 
                           const SplitKKTResidual& kkt_residual, 
                           UnconstrOCPData& data, SplitDirection& d) const; 

  ///
  /// @brief Returns maximum stap size of the primal variables that satisfies 
  /// the inequality constraints.
  /// @return Maximum stap size of the primal variables that satisfies 
  /// the inequality constraints.
  ///
  double maxPrimalStepSize(const UnconstrOCPData& data) const;

  ///
  /// @brief Returns maximum stap size of the dual variables that satisfies 
  /// the inequality constraints.
  /// @return Maximum stap size of the dual variables that satisfies 
  /// the inequality constraints.
  ///
  double maxDualStepSize(const UnconstrOCPData& data) const;

  ///
  /// @brief Updates primal variables of this stage.
  /// @param[in] robot Robot model. 
  /// @param[in] primal_step_size Primal step size. 
  /// @param[in] d Split direction of this stage.
  /// @param[in, out] s Split solution of this stage.
  ///
  void updatePrimal(const Robot& robot, const double primal_step_size, 
                    const SplitDirection& d, SplitSolution& s, 
                    UnconstrOCPData& data) const;

  ///
  /// @brief Updates dual variables of the inequality constraints.
  /// @param[in] dual_step_size Dula step size. 
  ///
  void updateDual(const double dual_step_size, UnconstrOCPData& data) const;

  ///
  /// @brief Computes the terminal cost Hessian to initialize the auxiliary 
  /// matrices for backward correction. 
  /// @param[in] robot Robot model. 
  /// @param[in] grid_info Grid info. 
  /// @param[in] s Split solution of this time stage.
  /// @param[in, out] kkt_matrix Split KKT matrix of this time stage.
  /// @param[in, out] kkt_residual Split KKT residual of this time stage.
  ///
  void evalTerminalCostHessian(Robot& robot, const GridInfo& grid_info, 
                               const SplitSolution& s, 
                               UnconstrOCPData& data,
                               SplitKKTMatrix& kkt_matrix,
                               SplitKKTResidual& kkt_residual) const;

private:
  std::shared_ptr<CostFunction> cost_;
  std::shared_ptr<Constraints> constraints_;
  ContactStatus contact_status_;
};

} // namespace robotoc

#endif // ROBOTOC_UNCONSTR_PARNMPC_STAGE_HPP_