#include <memory>

#include <gtest/gtest.h>
#include "Eigen/Core"

#include "idocp/robot/robot.hpp"
#include "idocp/cost/time_varying_task_space_3d_cost.hpp"
#include "idocp/cost/cost_function_data.hpp"
#include "idocp/ocp/split_solution.hpp"
#include "idocp/ocp/split_kkt_residual.hpp"
#include "idocp/ocp/split_kkt_matrix.hpp"

#include "idocp/utils/derivative_checker.hpp"

#include "robot_factory.hpp"

namespace idocp {
  
class TimeVaryingTaskSpace3DRef final : public TimeVaryingTaskSpace3DRefBase {
public:
  TimeVaryingTaskSpace3DRef(const Eigen::Vector3d& q0_ref, 
                            const Eigen::Vector3d& v_ref, 
                            const double t0, const double tf)
    : q0_ref_(q0_ref),
      v_ref_(v_ref),
      t0_(t0),
      tf_(tf) {
  }

  TimeVaryingTaskSpace3DRef() {}

  ~TimeVaryingTaskSpace3DRef() {}

  TimeVaryingTaskSpace3DRef(const TimeVaryingTaskSpace3DRef&) = default;

  TimeVaryingTaskSpace3DRef& operator=( 
      const TimeVaryingTaskSpace3DRef&) = default;

  TimeVaryingTaskSpace3DRef(
      TimeVaryingTaskSpace3DRef&&) noexcept = default;

  TimeVaryingTaskSpace3DRef& operator=(
      TimeVaryingTaskSpace3DRef&&) noexcept = default;

  void update_q_3d_ref(const double t, Eigen::VectorXd& q_ref) const override {
    q_ref = q0_ref_ + (t-t0_) * v_ref_;
  }

  bool isActive(const double t) const override {
    if (t0_ <= t && t <= tf_)
      return true;
    else 
      return false;
  }

private:
  Eigen::Vector3d q0_ref_, v_ref_;
  double t0_, tf_;
};


class TimeVaryingTaskSpace3DCostTest : public ::testing::Test {
protected:
  virtual void SetUp() {
    srand((unsigned int) time(0));
    std::random_device rnd;
    t = std::abs(Eigen::VectorXd::Random(1)[0]);
    dt = std::abs(Eigen::VectorXd::Random(1)[0]);
    t0 = t - std::abs(Eigen::VectorXd::Random(1)[0]);
    tf = t + std::abs(Eigen::VectorXd::Random(1)[0]);
  }

  virtual void TearDown() {
  }

  void testStageCost(Robot& robot, const int frame_id) const;
  void testTerminalCost(Robot& robot, const int frame_id) const;
  void testImpulseCost(Robot& robot, const int frame_id) const;

  double dt, t, t0, tf;
};


void TimeVaryingTaskSpace3DCostTest::testStageCost(Robot& robot, const int frame_id) const {
  const int dimv = robot.dimv();
  SplitKKTMatrix kkt_mat(robot);
  SplitKKTResidual kkt_res(robot);
  kkt_mat.Qqq().setRandom();
  kkt_mat.Qvv().setRandom();
  kkt_mat.Qaa().setRandom();
  kkt_mat.Quu().setRandom();
  kkt_res.lq().setRandom();
  kkt_res.lv().setRandom();
  kkt_res.la.setRandom();
  kkt_res.lu().setRandom();
  SplitKKTMatrix kkt_mat_ref = kkt_mat;
  SplitKKTResidual kkt_res_ref = kkt_res;
  const Eigen::Vector3d q_weight = Eigen::Vector3d::Random().array().abs();
  const Eigen::Vector3d qf_weight = Eigen::Vector3d::Random().array().abs();
  const Eigen::Vector3d qi_weight = Eigen::Vector3d::Random().array().abs();
  const Eigen::Vector3d q0_ref = Eigen::Vector3d::Random();
  const Eigen::Vector3d v_ref = Eigen::Vector3d::Random();
  auto ref = std::make_shared<TimeVaryingTaskSpace3DRef>(q0_ref, v_ref, t0, tf);
  auto cost = std::make_shared<TimeVaryingTaskSpace3DCost>(robot, frame_id, ref);

  CostFunctionData data(robot);
  EXPECT_TRUE(cost->useKinematics());
  cost->set_q_weight(q_weight);
  cost->set_qf_weight(qf_weight);
  cost->set_qi_weight(qi_weight);
  const SplitSolution s = SplitSolution::Random(robot);
  robot.updateKinematics(s.q, s.v, s.a);

  EXPECT_DOUBLE_EQ(cost->computeStageCost(robot, data, t0-dt, dt, s), 0);
  EXPECT_DOUBLE_EQ(cost->computeStageCost(robot, data, tf+dt, dt, s), 0);
  cost->computeStageCostDerivatives(robot, data, t0-dt, dt, s, kkt_res);
  EXPECT_TRUE(kkt_res.isApprox(kkt_res_ref));
  cost->computeStageCostDerivatives(robot, data, tf+dt, dt, s, kkt_res);
  EXPECT_TRUE(kkt_res.isApprox(kkt_res_ref));
  cost->computeStageCostHessian(robot, data, t0-dt, dt, s, kkt_mat);
  EXPECT_TRUE(kkt_mat.isApprox(kkt_mat_ref));
  cost->computeStageCostHessian(robot, data, tf+dt, dt, s, kkt_mat);
  EXPECT_TRUE(kkt_mat.isApprox(kkt_mat_ref));

  const Eigen::Vector3d q_ref = q0_ref + (t-t0) * v_ref;
  const Eigen::Vector3d q_task = robot.framePosition(frame_id);
  const Eigen::Vector3d q_diff = q_task - q_ref;
  const double l_ref = dt * 0.5 * q_diff.transpose() * q_weight.asDiagonal() * q_diff;
  EXPECT_DOUBLE_EQ(cost->computeStageCost(robot, data, t, dt, s), l_ref);
  cost->computeStageCostDerivatives(robot, data, t, dt, s, kkt_res);
  cost->computeStageCostHessian(robot, data, t, dt, s, kkt_mat);
  Eigen::MatrixXd J_6d = Eigen::MatrixXd::Zero(6, dimv);
  robot.getFrameJacobian(frame_id, J_6d);
  const Eigen::MatrixXd J_diff = robot.frameRotation(frame_id) * J_6d.topRows(3);
  kkt_res_ref.lq() += dt * J_diff.transpose() * q_weight.asDiagonal() * q_diff;
  kkt_mat_ref.Qqq() += dt * J_diff.transpose() * q_weight.asDiagonal() * J_diff;
  EXPECT_TRUE(kkt_res.isApprox(kkt_res_ref));
  EXPECT_TRUE(kkt_mat.isApprox(kkt_mat_ref));
  DerivativeChecker derivative_checker(robot);
  EXPECT_TRUE(derivative_checker.checkFirstOrderStageCostDerivatives(cost));
}


void TimeVaryingTaskSpace3DCostTest::testTerminalCost(Robot& robot, const int frame_id) const {
  const int dimv = robot.dimv();
  SplitKKTMatrix kkt_mat(robot);
  SplitKKTResidual kkt_res(robot);
  kkt_mat.Qqq().setRandom();
  kkt_mat.Qvv().setRandom();
  kkt_mat.Qaa().setRandom();
  kkt_mat.Quu().setRandom();
  kkt_res.lq().setRandom();
  kkt_res.lv().setRandom();
  kkt_res.la.setRandom();
  kkt_res.lu().setRandom();
  SplitKKTMatrix kkt_mat_ref = kkt_mat;
  SplitKKTResidual kkt_res_ref = kkt_res;
  const Eigen::Vector3d q_weight = Eigen::Vector3d::Random().array().abs();
  const Eigen::Vector3d qf_weight = Eigen::Vector3d::Random().array().abs();
  const Eigen::Vector3d qi_weight = Eigen::Vector3d::Random().array().abs();
  const Eigen::Vector3d q0_ref = Eigen::Vector3d::Random();
  const Eigen::Vector3d v_ref = Eigen::Vector3d::Random();
  auto ref = std::make_shared<TimeVaryingTaskSpace3DRef>(q0_ref, v_ref, t0, tf);
  auto cost = std::make_shared<TimeVaryingTaskSpace3DCost>(robot, frame_id, ref);

  CostFunctionData data(robot);
  EXPECT_TRUE(cost->useKinematics());
  cost->set_q_weight(q_weight);
  cost->set_qf_weight(qf_weight);
  cost->set_qi_weight(qi_weight);
  const SplitSolution s = SplitSolution::Random(robot);
  robot.updateKinematics(s.q, s.v, s.a);

  EXPECT_DOUBLE_EQ(cost->computeTerminalCost(robot, data, t0-dt, s), 0);
  EXPECT_DOUBLE_EQ(cost->computeTerminalCost(robot, data, tf+dt, s), 0);
  cost->computeTerminalCostDerivatives(robot, data, t0-dt, s, kkt_res);
  EXPECT_TRUE(kkt_res.isApprox(kkt_res_ref));
  cost->computeTerminalCostDerivatives(robot, data, tf+dt, s, kkt_res);
  EXPECT_TRUE(kkt_res.isApprox(kkt_res_ref));
  cost->computeTerminalCostHessian(robot, data, t0-dt, s, kkt_mat);
  EXPECT_TRUE(kkt_mat.isApprox(kkt_mat_ref));
  cost->computeTerminalCostHessian(robot, data, tf+dt, s, kkt_mat);
  EXPECT_TRUE(kkt_mat.isApprox(kkt_mat_ref));

  const Eigen::Vector3d q_ref = q0_ref + (t-t0) * v_ref;
  const Eigen::Vector3d q_task = robot.framePosition(frame_id);
  const Eigen::Vector3d q_diff = q_task - q_ref;
  const double l_ref = 0.5 * q_diff.transpose() * qf_weight.asDiagonal() * q_diff;
  EXPECT_DOUBLE_EQ(cost->computeTerminalCost(robot, data, t, s), l_ref);
  cost->computeTerminalCostDerivatives(robot, data, t, s, kkt_res);
  cost->computeTerminalCostHessian(robot, data, t, s, kkt_mat);
  Eigen::MatrixXd J_6d = Eigen::MatrixXd::Zero(6, dimv);
  robot.getFrameJacobian(frame_id, J_6d);
  const Eigen::MatrixXd J_diff = robot.frameRotation(frame_id) * J_6d.topRows(3);
  kkt_res_ref.lq() += J_diff.transpose() * qf_weight.asDiagonal() * q_diff;
  kkt_mat_ref.Qqq() += J_diff.transpose() * qf_weight.asDiagonal() * J_diff;
  EXPECT_TRUE(kkt_res.isApprox(kkt_res_ref));
  EXPECT_TRUE(kkt_mat.isApprox(kkt_mat_ref));
  DerivativeChecker derivative_checker(robot);
  EXPECT_TRUE(derivative_checker.checkFirstOrderTerminalCostDerivatives(cost));
}


void TimeVaryingTaskSpace3DCostTest::testImpulseCost(Robot& robot, const int frame_id) const {
  const int dimv = robot.dimv();
  ImpulseSplitKKTMatrix kkt_mat(robot);
  ImpulseSplitKKTResidual kkt_res(robot);
  kkt_mat.Qqq().setRandom();
  kkt_mat.Qvv().setRandom();
  kkt_mat.Qdvdv().setRandom();
  kkt_res.lq().setRandom();
  kkt_res.lv().setRandom();
  kkt_res.ldv.setRandom();
  ImpulseSplitKKTMatrix kkt_mat_ref = kkt_mat;
  ImpulseSplitKKTResidual kkt_res_ref = kkt_res;
  const Eigen::Vector3d q_weight = Eigen::Vector3d::Random().array().abs();
  const Eigen::Vector3d qf_weight = Eigen::Vector3d::Random().array().abs();
  const Eigen::Vector3d qi_weight = Eigen::Vector3d::Random().array().abs();
  const Eigen::Vector3d q0_ref = Eigen::Vector3d::Random();
  const Eigen::Vector3d v_ref = Eigen::Vector3d::Random();
  auto ref = std::make_shared<TimeVaryingTaskSpace3DRef>(q0_ref, v_ref, t0, tf);
  auto cost = std::make_shared<TimeVaryingTaskSpace3DCost>(robot, frame_id, ref);

  CostFunctionData data(robot);
  EXPECT_TRUE(cost->useKinematics());
  cost->set_q_weight(q_weight);
  cost->set_qf_weight(qf_weight);
  cost->set_qi_weight(qi_weight);
  const ImpulseSplitSolution s = ImpulseSplitSolution::Random(robot);
  robot.updateKinematics(s.q, s.v);

  EXPECT_DOUBLE_EQ(cost->computeImpulseCost(robot, data, t0-dt, s), 0);
  EXPECT_DOUBLE_EQ(cost->computeImpulseCost(robot, data, tf+dt, s), 0);
  cost->computeImpulseCostDerivatives(robot, data, t0-dt, s, kkt_res);
  EXPECT_TRUE(kkt_res.isApprox(kkt_res_ref));
  cost->computeImpulseCostDerivatives(robot, data, tf+dt, s, kkt_res);
  EXPECT_TRUE(kkt_res.isApprox(kkt_res_ref));
  cost->computeImpulseCostHessian(robot, data, t0-dt, s, kkt_mat);
  EXPECT_TRUE(kkt_mat.isApprox(kkt_mat_ref));
  cost->computeImpulseCostHessian(robot, data, tf+dt, s, kkt_mat);
  EXPECT_TRUE(kkt_mat.isApprox(kkt_mat_ref));

  const Eigen::Vector3d q_ref = q0_ref + (t-t0) * v_ref;
  const Eigen::Vector3d q_task = robot.framePosition(frame_id);
  const Eigen::Vector3d q_diff = q_task - q_ref;
  const double l_ref = 0.5 * q_diff.transpose() * qi_weight.asDiagonal() * q_diff;
  EXPECT_DOUBLE_EQ(cost->computeImpulseCost(robot, data, t, s), l_ref);
  cost->computeImpulseCostDerivatives(robot, data, t, s, kkt_res);
  cost->computeImpulseCostHessian(robot, data, t, s, kkt_mat);
  Eigen::MatrixXd J_6d = Eigen::MatrixXd::Zero(6, dimv);
  robot.getFrameJacobian(frame_id, J_6d);
  const Eigen::MatrixXd J_diff = robot.frameRotation(frame_id) * J_6d.topRows(3);
  kkt_res_ref.lq() += J_diff.transpose() * qi_weight.asDiagonal() * q_diff;
  kkt_mat_ref.Qqq() += J_diff.transpose() * qi_weight.asDiagonal() * J_diff;
  EXPECT_TRUE(kkt_res.isApprox(kkt_res_ref));
  EXPECT_TRUE(kkt_mat.isApprox(kkt_mat_ref));
  DerivativeChecker derivative_checker(robot);
  EXPECT_TRUE(derivative_checker.checkFirstOrderImpulseCostDerivatives(cost));
}


TEST_F(TimeVaryingTaskSpace3DCostTest, fixedBase) {
  auto robot = testhelper::CreateFixedBaseRobot(dt);
  const int frame_id = robot.contactFrames()[0];
  testStageCost(robot, frame_id);
  testTerminalCost(robot, frame_id);
  testImpulseCost(robot, frame_id);
}


TEST_F(TimeVaryingTaskSpace3DCostTest, floatingBase) {
  auto robot = testhelper::CreateFloatingBaseRobot(dt);
  const std::vector<int> frames = robot.contactFrames();
  for (const auto frame_id : frames) {
    testStageCost(robot, frame_id);
    testTerminalCost(robot, frame_id);
    testImpulseCost(robot, frame_id);
  }
}

} // namespace idocp


int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}