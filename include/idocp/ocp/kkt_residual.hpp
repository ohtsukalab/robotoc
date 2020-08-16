#ifndef IDOCP_KKT_RESIDUAL_HPP_
#define IDOCP_KKT_RESIDUAL_HPP_

#include <assert.h>

#include "Eigen/Core"

#include "idocp/robot/robot.hpp"
#include "idocp/ocp/kkt_composition.hpp"


namespace idocp {

class KKTResidual {
public:
  EIGEN_MAKE_ALIGNED_OPERATOR_NEW

  KKTResidual(const Robot& robot) 
    : kkt_composition_(robot),
      kkt_residual_(Eigen::VectorXd::Zero(kkt_composition_.max_dimKKT())) {
  }

  KKTResidual() 
    : kkt_composition_(),
      kkt_residual_() {
  }

  ~KKTResidual() {
  }

  KKTResidual(const KKTResidual&) = default;

  KKTResidual& operator=(const KKTResidual&) = default;
 
  KKTResidual(KKTResidual&&) noexcept = default;

  KKTResidual& operator=(KKTResidual&&) noexcept = default;

  inline void setContactStatus(const Robot& robot) {
    kkt_composition_.setContactStatus(robot);
  }

  inline Eigen::Ref<Eigen::VectorXd> KKT_residual() {
    return kkt_residual_.head(kkt_composition_.dimKKT());
  }

  inline Eigen::Ref<Eigen::VectorXd> Fq() {
    return kkt_residual_.segment(kkt_composition_.Fq_begin(), 
                                 kkt_composition_.Fq_size());
  }

  inline Eigen::Ref<Eigen::VectorXd> Fv() {
    return kkt_residual_.segment(kkt_composition_.Fv_begin(), 
                                 kkt_composition_.Fv_size());
  }

  template <typename Derived>
  inline Eigen::MatrixBase<Derived>& C() {
    return kkt_residual_.segment(kkt_composition_.C_begin(), 
                                 kkt_composition_.C_size());
  }


  // inline Eigen::Ref<Eigen::VectorXd> C() {
  //   return kkt_residual_.segment(kkt_composition_.C_begin(), 
  //                                kkt_composition_.C_size());
  // }

  inline Eigen::Ref<Eigen::VectorXd> la() {
    return kkt_residual_.segment(kkt_composition_.Qa_begin(), 
                                 kkt_composition_.Qa_size());
  }

  inline Eigen::Ref<Eigen::VectorXd> lf() {
    return kkt_residual_.segment(kkt_composition_.Qf_begin(), 
                                 kkt_composition_.Qf_size());
  }

  inline Eigen::Ref<Eigen::VectorXd> lq() {
    return kkt_residual_.segment(kkt_composition_.Qq_begin(), 
                                 kkt_composition_.Qq_size());
  }

  inline Eigen::Ref<Eigen::VectorXd> lv() {
    return kkt_residual_.segment(kkt_composition_.Qv_begin(), 
                                 kkt_composition_.Qv_size());
  }

  inline Eigen::Ref<Eigen::VectorXd> lx() {
    return kkt_residual_.segment(kkt_composition_.Qx_begin(), 
                                 kkt_composition_.Qx_size());
  }

  inline Eigen::Ref<const Eigen::VectorXd> KKT_residual() const {
    return kkt_residual_.head(kkt_composition_.dimKKT());
  }

  inline Eigen::Ref<const Eigen::VectorXd> Fq() const {
    return kkt_residual_.segment(kkt_composition_.Fq_begin(), 
                                 kkt_composition_.Fq_size());
  }

  inline Eigen::Ref<const Eigen::VectorXd> Fv() const {
    return kkt_residual_.segment(kkt_composition_.Fv_begin(), 
                                 kkt_composition_.Fv_size());
  }

  // inline Eigen::Ref<const Eigen::VectorXd> C() const {
  //   return kkt_residual_.segment(kkt_composition_.C_begin(), 
  //                                kkt_composition_.C_size());
  // }

  inline Eigen::Ref<const Eigen::VectorXd> la() const {
    return kkt_residual_.segment(kkt_composition_.Qa_begin(), 
                                 kkt_composition_.Qa_size());
  }

  inline Eigen::Ref<const Eigen::VectorXd> lf() const {
    return kkt_residual_.segment(kkt_composition_.Qf_begin(), 
                                 kkt_composition_.Qf_size());
  }

  inline Eigen::Ref<const Eigen::VectorXd> lq() const {
    return kkt_residual_.segment(kkt_composition_.Qq_begin(), 
                                 kkt_composition_.Qq_size());
  }

  inline Eigen::Ref<const Eigen::VectorXd> lv() const {
    return kkt_residual_.segment(kkt_composition_.Qv_begin(), 
                                 kkt_composition_.Qv_size());
  }

  inline Eigen::Ref<const Eigen::VectorXd> lx() const {
    return kkt_residual_.segment(kkt_composition_.Qx_begin(), 
                                 kkt_composition_.Qx_size());
  }

  inline double squaredKKTErrorNorm() const {
    return kkt_residual_.head(kkt_composition_.dimKKT()).squaredNorm();
  }

  inline void setZero() {
    kkt_residual_.setZero();
  }

  inline int dimKKT() const {
    return kkt_composition_.dimKKT();
  }

  inline int max_dimKKT() const {
    return kkt_composition_.max_dimKKT();
  }

private:
  KKTComposition kkt_composition_;
  Eigen::VectorXd kkt_residual_;

};

} // namespace idocp 


#endif // IDOCP_KKT_RESIDUAL_HPP_