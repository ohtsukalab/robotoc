#ifndef IDOCP_KKT_RESIDUAL_HXX_
#define IDOCP_KKT_RESIDUAL_HXX_

#include "idocp/ocp/kkt_residual.hpp"

namespace idocp {

inline KKTResidual::KKTResidual(const Robot& robot) 
  : KKT_residual(Eigen::VectorXd::Zero(5*robot.dimv()-robot.dim_passive())),
    la(Eigen::VectorXd::Zero(robot.dimv())),
    ID(Eigen::VectorXd::Zero(robot.dimv())),
    lu_passive(Eigen::VectorXd::Zero(robot.dim_passive())),
    C_passive(Eigen::VectorXd::Zero(robot.dim_passive())),
    C_full_(Eigen::VectorXd::Zero(robot.max_dimf())),
    lf_full_(Eigen::VectorXd::Zero(robot.max_dimf())),
    dimv_(robot.dimv()), 
    dimx_(2*robot.dimv()), 
    dimu_(robot.dimv()-robot.dim_passive()),
    dim_passive_(robot.dim_passive()),
    dimf_(0), 
    dimKKT_(5*robot.dimv()+robot.dim_passive()) {
}


inline KKTResidual::KKTResidual() 
  : KKT_residual(),
    la(),
    ID(),
    lu_passive(),
    C_passive(),
    C_full_(),
    lf_full_(),
    dimv_(0), 
    dimx_(0), 
    dimu_(0),
    dim_passive_(0),
    dimf_(0), 
    dimKKT_(0) {
}


inline KKTResidual::~KKTResidual() {
}


inline void KKTResidual::setContactStatus(const ContactStatus& contact_status) {
  dimf_ = contact_status.dimf();
}


inline Eigen::VectorBlock<Eigen::VectorXd> KKTResidual::Fq() {
  return KKT_residual.head(dimv_);
}


inline const Eigen::VectorBlock<const Eigen::VectorXd> KKTResidual::Fq() const {
  return KKT_residual.head(dimv_);
}


inline Eigen::VectorBlock<Eigen::VectorXd> KKTResidual::Fv() {
  return KKT_residual.segment(dimv_, dimv_);
}


inline const Eigen::VectorBlock<const Eigen::VectorXd> KKTResidual::Fv() const {
  return KKT_residual.segment(dimv_, dimv_);
}


inline Eigen::VectorBlock<Eigen::VectorXd> KKTResidual::Fx() {
  return KKT_residual.head(dimx_);
}


inline const Eigen::VectorBlock<const Eigen::VectorXd> KKTResidual::Fx() const {
  return KKT_residual.head(dimx_);
}


inline Eigen::VectorBlock<Eigen::VectorXd> KKTResidual::lu() {
  return KKT_residual.segment(dimx_, dimu_);
}


inline const Eigen::VectorBlock<const Eigen::VectorXd> KKTResidual::lu() const {
  return KKT_residual.segment(dimx_, dimu_);
}


inline Eigen::VectorBlock<Eigen::VectorXd> KKTResidual::lq() {
  return KKT_residual.segment(dimx_+dimu_, dimv_);
}


inline const Eigen::VectorBlock<const Eigen::VectorXd> KKTResidual::lq() const {
  return KKT_residual.segment(dimx_+dimu_, dimv_);
}


inline Eigen::VectorBlock<Eigen::VectorXd> KKTResidual::lv() {
  return KKT_residual.segment(dimx_+dimu_+dimv_, dimv_);
}


inline const Eigen::VectorBlock<const Eigen::VectorXd> KKTResidual::lv() const {
  return KKT_residual.segment(dimx_+dimu_+dimv_, dimv_);
}


inline Eigen::VectorBlock<Eigen::VectorXd> KKTResidual::lx() {
  return KKT_residual.segment(dimx_+dimu_, dimx_);
}


inline const Eigen::VectorBlock<const Eigen::VectorXd> KKTResidual::lx() const {
  return KKT_residual.segment(dimx_+dimu_, dimx_);
}


inline Eigen::VectorBlock<Eigen::VectorXd> KKTResidual::C() {
  return C_full_.head(dimf_);
}


inline const Eigen::VectorBlock<const Eigen::VectorXd> KKTResidual::C() const {
  return C_full_.head(dimf_);
}


inline Eigen::VectorBlock<Eigen::VectorXd> KKTResidual::lf() {
  return lf_full_.head(dimf_);
}


inline const Eigen::VectorBlock<const Eigen::VectorXd> KKTResidual::lf() const {
  return lf_full_.head(dimf_);
}


inline void KKTResidual::setZero() {
  KKT_residual.setZero();
  la.setZero();
  ID.setZero();
  lu_passive.setZero();
  C_passive.setZero();
  C_full_.setZero();
  lf_full_.setZero();
}


inline int KKTResidual::dimKKT() const {
  return dimKKT_;
}


inline int KKTResidual::dimf() const {
  return dimf_;
}

} // namespace idocp 

#endif // IDOCP_KKT_RESIDUAL_HXX_