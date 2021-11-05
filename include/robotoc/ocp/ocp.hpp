#ifndef ROBOTOC_OCP_HPP_
#define ROBOTOC_OCP_HPP_

#include <vector>
#include <memory>
#include <cassert>

#include "robotoc/robot/robot.hpp"
#include "robotoc/ocp/split_ocp.hpp"
#include "robotoc/impulse/impulse_split_ocp.hpp"
#include "robotoc/ocp/terminal_ocp.hpp"
#include "robotoc/cost/cost_function.hpp"
#include "robotoc/constraints/constraints.hpp"
#include "robotoc/hybrid/contact_sequence.hpp"
#include "robotoc/hybrid/hybrid_ocp_discretization.hpp"


namespace robotoc {

///
/// @class OCP
/// @brief The (hybrid) optimal control problem.
///
class OCP {
public:
  ///
  /// @brief Construct the optiaml control problem. 
  /// @param[in] robot Robot model. 
  /// @param[in] cost Shared ptr to the cost function.
  /// @param[in] constraints Shared ptr to the constraints.
  /// @param[in] T Length of the horzion.
  /// @param[in] N Number of the discretization grids.
  /// @param[in] N_impulse Maximum number of the impulses on the horizon. 
  /// Default is 0.
  ///
  OCP(const Robot& robot, const std::shared_ptr<CostFunction>& cost, 
      const std::shared_ptr<Constraints>& constraints, const double T, 
      const int N, const int N_impulse=0) 
    : data(N, SplitOCP(robot, cost, constraints)), 
      aux(N_impulse, SplitOCP(robot, cost, constraints)), 
      lift(N_impulse, SplitOCP(robot, cost, constraints)),
      impulse(N_impulse, ImpulseSplitOCP(robot, cost, constraints)),
      terminal(TerminalOCP(robot, cost, constraints)),
      time_discretization_(T, N, N_impulse),
      T_(T),
      N_(N) {
  }

  ///
  /// @brief Default Constructor.
  ///
  OCP() 
    : data(), 
      aux(),
      lift(),
      impulse(),
      terminal(),
      time_discretization_(),
      T_(0),
      N_(0) {
  }

  ///
  /// @brief Default copy constructor. 
  ///
  OCP(const OCP&) = default;

  ///
  /// @brief Default copy assign operator. 
  ///
  OCP& operator=(const OCP&) = default;

  ///
  /// @brief Default move constructor. 
  ///
  OCP(OCP&&) noexcept = default;

  ///
  /// @brief Default move assign operator. 
  ///
  OCP& operator=(OCP&&) noexcept = default;

  ///
  /// @brief Overload operator[] to access the standard data, i.e., 
  /// OCP::data as std::vector. 
  ///
  SplitOCP& operator[] (const int i) {
    assert(i >= 0);
    assert(i < data.size());
    return data[i];
  }

  ///
  /// @brief const version of OCP::operator[]. 
  ///
  const SplitOCP& operator[] (const int i) const {
    assert(i >= 0);
    assert(i < data.size());
    return data[i];
  }

  void setDiscretizationMethod(
      const DiscretizationMethod discretization_method) {
    time_discretization_.setDiscretizationMethod(discretization_method);
  }

  void discretize(const std::shared_ptr<ContactSequence>& contact_sequence, 
                  const double t) {
    time_discretization_.discretize(contact_sequence, t);
  }

  void meshRefinement(const std::shared_ptr<ContactSequence>& contact_sequence, 
                      const double t) {
    time_discretization_.meshRefinement(contact_sequence, t);
  }

  const HybridOCPDiscretization& discrete() const {
    return time_discretization_;
  }

  double T() const {
    return T_;
  }

  int N() const {
    return N_;
  }

  std::vector<SplitOCP> data, aux, lift;
  std::vector<ImpulseSplitOCP> impulse;
  TerminalOCP terminal;

private:
  HybridOCPDiscretization time_discretization_;
  double T_;
  int N_;

};

} // namespace robotoc

#endif // ROBOTOC_OCP_HPP_ 