#ifndef IDOCP_CONSTRAINTS_HXX_
#define IDOCP_CONSTRAINTS_HXX_

#include <assert.h>

namespace idocp {

inline Constraints::Constraints() 
  : constraints_() {
}


inline Constraints::~Constraints() {
}


inline void Constraints::push_back(
    const std::shared_ptr<ConstraintComponentBase>& constraint) {
  constraints_.push_back(constraint);
}


inline void Constraints::clear() {
  constraints_.clear();
}


inline bool Constraints::isEmpty() const {
  return constraints_.empty();
}


inline ConstraintsData Constraints::createConstraintsData(
    const Robot& robot) const {
  ConstraintsData datas;
  for (int i=0; i<constraints_.size(); ++i) {
    datas.data.push_back(ConstraintComponentData(constraints_[i]->dimc()));
  }
  return datas;
}


inline bool Constraints::isFeasible(const Robot& robot, ConstraintsData& datas, 
                                    const SplitSolution& s) const {
  for (int i=0; i<constraints_.size(); ++i) {
    bool feasible = constraints_[i]->isFeasible(robot, datas.data[i], s);
    if (!feasible) {
      return false;
    }
  }
  return true;
}


inline void Constraints::setSlackAndDual(const Robot& robot, 
                                         ConstraintsData& datas, 
                                         const double dtau, 
                                         const SplitSolution& s) const {
  for (int i=0; i<constraints_.size(); ++i) {
    constraints_[i]->setSlackAndDual(robot, datas.data[i], dtau, s);
  }
}


inline void Constraints::augmentDualResidual(const Robot& robot, 
                                             ConstraintsData& datas, 
                                             const double dtau, 
                                             KKTResidual& kkt_residual) const {
  for (int i=0; i<constraints_.size(); ++i) {
    constraints_[i]->augmentDualResidual(robot, datas.data[i], dtau, 
                                          kkt_residual);
  }
}


inline void Constraints::augmentDualResidual(const Robot& robot, 
                                             ConstraintsData& datas, 
                                             const double dtau, 
                                             Eigen::VectorXd& lu) const {
  assert(lu.size() == robot.dimv());
  for (int i=0; i<constraints_.size(); ++i) {
    constraints_[i]->augmentDualResidual(robot, datas.data[i], dtau, lu);
  }
}


inline void Constraints::condenseSlackAndDual(const Robot& robot,
                                              ConstraintsData& datas, 
                                              const double dtau, 
                                              const SplitSolution& s,
                                              KKTMatrix& kkt_matrix, 
                                              KKTResidual& kkt_residual) const {
  for (int i=0; i<constraints_.size(); ++i) {
    constraints_[i]->condenseSlackAndDual(robot, datas.data[i], dtau, s,
                                          kkt_matrix, kkt_residual);
  }
}


inline void Constraints::condenseSlackAndDual(const Robot& robot,
                                              ConstraintsData& datas, 
                                              const double dtau, 
                                              const Eigen::VectorXd& u,
                                              Eigen::MatrixXd& Quu, 
                                              Eigen::VectorXd& lu) const {
  assert(u.size() == robot.dimv());
  assert(Quu.rows() == robot.dimv());
  assert(Quu.cols() == robot.dimv());
  assert(lu.size() == robot.dimv());
  for (int i=0; i<constraints_.size(); ++i) {
    constraints_[i]->condenseSlackAndDual(robot, datas.data[i], dtau, u, Quu, lu);
  }
}


inline void Constraints::computeSlackAndDualDirection(
    const Robot& robot, ConstraintsData& datas, const double dtau, 
    const SplitDirection& d) const {
  for (int i=0; i<constraints_.size(); ++i) {
    constraints_[i]->computeSlackAndDualDirection(robot, datas.data[i], dtau, d);
  }
}


inline double Constraints::maxSlackStepSize(const ConstraintsData& datas) const {
  double min_step_size = 1;
  for (int i=0; i<constraints_.size(); ++i) {
    const double step_size = constraints_[i]->maxSlackStepSize(datas.data[i]);
    if (step_size < min_step_size) {
      min_step_size = step_size;
    }
  }
  return min_step_size;
}


inline double Constraints::maxDualStepSize(const ConstraintsData& datas) const {
  double min_step_size = 1;
  for (int i=0; i<constraints_.size(); ++i) {
    const double step_size = constraints_[i]->maxDualStepSize(datas.data[i]);
    if (step_size < min_step_size) {
      min_step_size = step_size;
    }
  }
  return min_step_size;
}


inline void Constraints::updateSlack(ConstraintsData& datas, 
                                     const double step_size) const {
  for (int i=0; i<constraints_.size(); ++i) {
    constraints_[i]->updateSlack(datas.data[i], step_size);
  }
}


inline void Constraints::updateDual(ConstraintsData& datas, 
                                    const double step_size) const {
  for (int i=0; i<constraints_.size(); ++i) {
    constraints_[i]->updateDual(datas.data[i], step_size);
  }
}


inline double Constraints::costSlackBarrier(const ConstraintsData& datas) const {
  double cost = 0;
  for (int i=0; i<constraints_.size(); ++i) {
    cost += constraints_[i]->costSlackBarrier(datas.data[i]);
  }
  return cost;
}


inline double Constraints::costSlackBarrier(const ConstraintsData& datas, 
                                            const double step_size) const {
  double cost = 0;
  for (int i=0; i<constraints_.size(); ++i) {
    cost += constraints_[i]->costSlackBarrier(datas.data[i], step_size);
  }
  return cost;
}


inline double Constraints::residualL1Nrom(const Robot& robot, 
                                          ConstraintsData& datas, 
                                          const double dtau, 
                                          const SplitSolution& s) const {
  double l1_norm = 0;
  for (int i=0; i<constraints_.size(); ++i) {
    l1_norm += constraints_[i]->residualL1Nrom(robot, datas.data[i], dtau, s);
  }
  return l1_norm;
}


inline double Constraints::squaredKKTErrorNorm(const Robot& robot, 
                                               ConstraintsData& datas, 
                                               const double dtau, 
                                               const SplitSolution& s) const {
  double squared_norm = 0;
  for (int i=0; i<constraints_.size(); ++i) {
    squared_norm += constraints_[i]->squaredKKTErrorNorm(robot, datas.data[i],
                                                          dtau, s);
  }
  return squared_norm;
}

} // namespace idocp

#endif // IDOCP_CONSTRAINTS_HXX_