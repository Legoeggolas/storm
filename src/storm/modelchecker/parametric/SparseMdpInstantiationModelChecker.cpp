#include "SparseMdpInstantiationModelChecker.h"

#include "storm/logic/FragmentSpecification.h"
#include "storm/modelchecker/results/ExplicitQuantitativeCheckResult.h"
#include "storm/modelchecker/hints/ExplicitModelCheckerHint.h"

#include "storm/exceptions/InvalidArgumentException.h"
#include "storm/exceptions/InvalidStateException.h"

namespace storm {
    namespace modelchecker {
        namespace parametric {
            
            template <typename SparseModelType, typename ConstantType>
            SparseMdpInstantiationModelChecker<SparseModelType, ConstantType>::SparseMdpInstantiationModelChecker(SparseModelType const& parametricModel) : SparseInstantiationModelChecker<SparseModelType, ConstantType>(parametricModel), modelInstantiator(parametricModel) {
                //Intentionally left empty
            }
    
           template <typename SparseModelType, typename ConstantType>
            std::unique_ptr<CheckResult> SparseMdpInstantiationModelChecker<SparseModelType, ConstantType>::check(storm::utility::parametric::Valuation<typename SparseModelType::ValueType> const& valuation) {
                STORM_LOG_THROW(this->currentCheckTask, storm::exceptions::InvalidStateException, "Checking has been invoked but no property has been specified before.");
                auto const& instantiatedModel = modelInstantiator.instantiate(valuation);
                storm::modelchecker::SparseMdpPrctlModelChecker<storm::models::sparse::Mdp<ConstantType>> modelChecker(instantiatedModel);

                // Check if there are some optimizations implemented for the specified property
                if(!this->currentCheckTask->isQualitativeSet() && this->currentCheckTask->getFormula().isInFragment(storm::logic::reachability().setRewardOperatorsAllowed(true).setReachabilityRewardFormulasAllowed(true))) {
                    return checkWithResultHint(modelChecker);
                } else {
                    return modelChecker.check(*this->currentCheckTask);
                }
            }
            
                
            template <typename SparseModelType, typename ConstantType>
            std::unique_ptr<CheckResult> SparseMdpInstantiationModelChecker<SparseModelType, ConstantType>::checkWithResultHint(storm::modelchecker::SparseMdpPrctlModelChecker<storm::models::sparse::Mdp<ConstantType>>& modelchecker) {
                
                this->currentCheckTask->setProduceSchedulers(true);
                
                // Check the formula and store the result as a hint for the next call.
                // For qualitative properties, we still want a quantitative result hint. Hence we perform the check on the subformula
                if(this->currentCheckTask->getFormula().asOperatorFormula().hasQuantitativeResult()) {
                    std::unique_ptr<storm::modelchecker::CheckResult> result = modelchecker.check(*this->currentCheckTask);
                    storm::storage::Scheduler const& scheduler = result->template asExplicitQuantitativeCheckResult<ConstantType>().getScheduler();
                    this->currentCheckTask->setHint(ExplicitModelCheckerHint<ConstantType>(result->template asExplicitQuantitativeCheckResult<ConstantType>().getValueVector(),
                                                                                           dynamic_cast<storm::storage::TotalScheduler const&>(scheduler)));
                    return result;
                } else {
                    std::unique_ptr<storm::modelchecker::CheckResult> quantitativeResult;
                    auto newCheckTask = this->currentCheckTask->substituteFormula(this->currentCheckTask->getFormula().asOperatorFormula().getSubformula()).setOnlyInitialStatesRelevant(false);
                    if(this->currentCheckTask->getFormula().isProbabilityOperatorFormula()) {
                        quantitativeResult = modelchecker.computeProbabilities(newCheckTask);
                    } else if (this->currentCheckTask->getFormula().isRewardOperatorFormula()) {
                        quantitativeResult = modelchecker.computeRewards(this->currentCheckTask->getFormula().asRewardOperatorFormula().getMeasureType(), newCheckTask);
                    } else {
                        STORM_LOG_THROW(false, storm::exceptions::InvalidArgumentException, "Checking with hint is only implemented for probability or reward operator formulas");
                    }
                    std::unique_ptr<storm::modelchecker::CheckResult> qualitativeResult = quantitativeResult->template asExplicitQuantitativeCheckResult<ConstantType>().compareAgainstBound(this->currentCheckTask->getFormula().asOperatorFormula().getComparisonType(), this->currentCheckTask->getFormula().asOperatorFormula().template getThresholdAs<ConstantType>());
                    storm::storage::Scheduler& scheduler = quantitativeResult->template asExplicitQuantitativeCheckResult<ConstantType>().getScheduler();
                    this->currentCheckTask->setHint(ExplicitModelCheckerHint<ConstantType>(std::move(quantitativeResult->template asExplicitQuantitativeCheckResult<ConstantType>().getValueVector()),
                                                                                           std::move(dynamic_cast<storm::storage::TotalScheduler const&>(scheduler))));
                    return qualitativeResult;
                }
            }
            
            template class SparseMdpInstantiationModelChecker<storm::models::sparse::Mdp<storm::RationalFunction>, double>;

        }
    }
}