#include "src/storage/bisimulation/BisimulationDecomposition.h"

#include <chrono>

#include "src/models/sparse/Dtmc.h"
#include "src/models/sparse/Ctmc.h"
#include "src/models/sparse/Mdp.h"
#include "src/models/sparse/StandardRewardModel.h"

#include "src/storage/bisimulation/DeterministicBlockData.h"

#include "src/modelchecker/propositional/SparsePropositionalModelChecker.h"
#include "src/modelchecker/results/ExplicitQualitativeCheckResult.h"

#include "src/settings/SettingsManager.h"
#include "src/settings/modules/MarkovChainSettings.h"

#include "src/logic/FormulaInformation.h"
#include "src/logic/FragmentSpecification.h"

#include "src/utility/macros.h"
#include "src/exceptions/IllegalFunctionCallException.h"
#include "src/exceptions/InvalidOptionException.h"

namespace storm {
    namespace storage {
        
        using namespace bisimulation;
        
        template<typename ModelType, typename BlockDataType>
        BisimulationDecomposition<ModelType, BlockDataType>::Options::Options(ModelType const& model, storm::logic::Formula const& formula) : Options() {
            this->preserveSingleFormula(model, formula);
        }
        
        template<typename ModelType, typename BlockDataType>
        BisimulationDecomposition<ModelType, BlockDataType>::Options::Options(ModelType const& model, std::vector<std::shared_ptr<storm::logic::Formula const>> const& formulas) : Options() {
            if (formulas.empty()) {
                this->respectedAtomicPropositions = model.getStateLabeling().getLabels();
                this->keepRewards = true;
            } if (formulas.size() == 1) {
                this->preserveSingleFormula(model, *formulas.front());
            } else {
                for (auto const& formula : formulas) {
                    preserveFormula(model, *formula);
                }
            }
        }
        
        template<typename ModelType, typename BlockDataType>
        BisimulationDecomposition<ModelType, BlockDataType>::Options::Options() : measureDrivenInitialPartition(false), phiStates(), psiStates(), respectedAtomicPropositions(), buildQuotient(true), keepRewards(false), type(BisimulationType::Strong), bounded(false) {
            // Intentionally left empty.
        }
        
        template<typename ModelType, typename BlockDataType>
        void BisimulationDecomposition<ModelType, BlockDataType>::Options::preserveFormula(ModelType const& model, storm::logic::Formula const& formula) {
            // Disable the measure driven initial partition.
            measureDrivenInitialPartition = false;
            phiStates = boost::none;
            psiStates = boost::none;
            
            // Retrieve information about formula.
            storm::logic::FormulaInformation info = formula.info();
            
            // Preserve rewards if necessary.
            keepRewards = keepRewards || info.containsRewardOperator();
            
            // Preserve bounded properties if necessary.
            bounded = bounded || (info.containsBoundedUntilFormula() || info.containsNextFormula());
            
            // Compute the relevant labels and expressions.
            this->addToRespectedAtomicPropositions(formula.getAtomicExpressionFormulas(), formula.getAtomicLabelFormulas());
        }
        
        template<typename ModelType, typename BlockDataType>
        void BisimulationDecomposition<ModelType, BlockDataType>::Options::preserveSingleFormula(ModelType const& model, storm::logic::Formula const& formula) {
            // Retrieve information about formula.
            storm::logic::FormulaInformation info = formula.info();

            keepRewards = info.containsRewardOperator();
            
            // We need to preserve bounded properties iff the formula contains a bounded until or a next subformula.
            bounded = info.containsBoundedUntilFormula() || info.containsNextFormula();
            
            // Compute the relevant labels and expressions.
            this->addToRespectedAtomicPropositions(formula.getAtomicExpressionFormulas(), formula.getAtomicLabelFormulas());

            // Check whether measure driven initial partition is possible and, if so, set it.
            this->checkAndSetMeasureDrivenInitialPartition(model, formula);
        }
        
        template<typename ModelType, typename BlockDataType>
        void BisimulationDecomposition<ModelType, BlockDataType>::Options::checkAndSetMeasureDrivenInitialPartition(ModelType const& model, storm::logic::Formula const& formula) {
            std::shared_ptr<storm::logic::Formula const> newFormula = formula.asSharedPointer();
            
            if (formula.isProbabilityOperatorFormula()) {
                if (formula.asProbabilityOperatorFormula().hasOptimalityType()) {
                    optimalityType = formula.asProbabilityOperatorFormula().getOptimalityType();
                } else if (formula.asProbabilityOperatorFormula().hasBound()) {
                    storm::logic::ComparisonType comparisonType = formula.asProbabilityOperatorFormula().getComparisonType();
                    if (comparisonType == storm::logic::ComparisonType::Less || comparisonType == storm::logic::ComparisonType::LessEqual) {
                        optimalityType = OptimizationDirection::Maximize;
                    } else {
                        optimalityType = OptimizationDirection::Minimize;
                    }
                }
                newFormula = formula.asProbabilityOperatorFormula().getSubformula().asSharedPointer();
            } else if (formula.isRewardOperatorFormula()) {
                if (formula.asRewardOperatorFormula().hasOptimalityType()) {
                    optimalityType = formula.asRewardOperatorFormula().getOptimalityType();
                } else if (formula.asRewardOperatorFormula().hasBound()) {
                    storm::logic::ComparisonType comparisonType = formula.asRewardOperatorFormula().getComparisonType();
                    if (comparisonType == storm::logic::ComparisonType::Less || comparisonType == storm::logic::ComparisonType::LessEqual) {
                        optimalityType = OptimizationDirection::Maximize;
                    } else {
                        optimalityType = OptimizationDirection::Minimize;
                    }
                }
                newFormula = formula.asRewardOperatorFormula().getSubformula().asSharedPointer();
            }
            
            std::shared_ptr<storm::logic::Formula const> leftSubformula = std::make_shared<storm::logic::BooleanLiteralFormula>(true);
            std::shared_ptr<storm::logic::Formula const> rightSubformula;
            if (newFormula->isUntilFormula()) {
                leftSubformula = newFormula->asUntilFormula().getLeftSubformula().asSharedPointer();
                rightSubformula = newFormula->asUntilFormula().getRightSubformula().asSharedPointer();
                if (leftSubformula->isInFragment(storm::logic::propositional()) && rightSubformula->isInFragment(storm::logic::propositional())) {
                    measureDrivenInitialPartition = true;
                }
            } else if (newFormula->isEventuallyFormula()) {
                rightSubformula = newFormula->asEventuallyFormula().getSubformula().asSharedPointer();
                if (rightSubformula->isInFragment(storm::logic::propositional())) {
                    measureDrivenInitialPartition = true;
                }
            }
            
            if (measureDrivenInitialPartition) {
                storm::modelchecker::SparsePropositionalModelChecker<ModelType> checker(model);
                std::unique_ptr<storm::modelchecker::CheckResult> phiStatesCheckResult = checker.check(*leftSubformula);
                std::unique_ptr<storm::modelchecker::CheckResult> psiStatesCheckResult = checker.check(*rightSubformula);
                phiStates = phiStatesCheckResult->asExplicitQualitativeCheckResult().getTruthValuesVector();
                psiStates = psiStatesCheckResult->asExplicitQualitativeCheckResult().getTruthValuesVector();
            } else {
                optimalityType = boost::none;
            }
        }
        
        template<typename ModelType, typename BlockDataType>
        void BisimulationDecomposition<ModelType, BlockDataType>::Options::addToRespectedAtomicPropositions(std::vector<std::shared_ptr<storm::logic::AtomicExpressionFormula const>> const& expressions, std::vector<std::shared_ptr<storm::logic::AtomicLabelFormula const>> const& labels) {
            std::set<std::string> labelsToRespect;
            for (auto const& labelFormula : labels) {
                labelsToRespect.insert(labelFormula->getLabel());
            }
            for (auto const& expressionFormula : expressions) {
                labelsToRespect.insert(expressionFormula->toString());
            }
            if (!respectedAtomicPropositions) {
                respectedAtomicPropositions = labelsToRespect;
            } else {
                respectedAtomicPropositions.get().insert(labelsToRespect.begin(), labelsToRespect.end());
            }
        }
        
        template<typename ModelType, typename BlockDataType>
        BisimulationDecomposition<ModelType, BlockDataType>::BisimulationDecomposition(ModelType const& model, Options const& options) : BisimulationDecomposition(model, model.getBackwardTransitions(), options) {
            // Intentionally left empty.
        }

        template<typename ModelType, typename BlockDataType>
        BisimulationDecomposition<ModelType, BlockDataType>::BisimulationDecomposition(ModelType const& model, storm::storage::SparseMatrix<ValueType> const& backwardTransitions, Options const& options) : model(model), backwardTransitions(backwardTransitions), options(options), partition(), comparator(), quotient(nullptr) {
            STORM_LOG_THROW(!options.getKeepRewards() || !model.hasRewardModel() || model.hasUniqueRewardModel(), storm::exceptions::IllegalFunctionCallException, "Bisimulation currently only supports models with at most one reward model.");
            STORM_LOG_THROW(!options.getKeepRewards() || !model.hasRewardModel() || model.getUniqueRewardModel()->second.hasOnlyStateRewards(), storm::exceptions::IllegalFunctionCallException, "Bisimulation is currently supported for models with state rewards only. Consider converting the transition rewards to state rewards (via suitable function calls).");
            STORM_LOG_THROW(options.getType() != BisimulationType::Weak || !options.getBounded(), storm::exceptions::IllegalFunctionCallException, "Weak bisimulation cannot preserve bounded properties.");
            
            // Fix the respected atomic propositions if they were not explicitly given.
            if (!this->options.respectedAtomicPropositions) {
                this->options.respectedAtomicPropositions = model.getStateLabeling().getLabels();
            }
        }
        
        template<typename ModelType, typename BlockDataType>
        void BisimulationDecomposition<ModelType, BlockDataType>::computeBisimulationDecomposition() {
            std::chrono::high_resolution_clock::time_point totalStart = std::chrono::high_resolution_clock::now();

            std::chrono::high_resolution_clock::time_point initialPartitionStart = std::chrono::high_resolution_clock::now();
            // initialize the initial partition.
            if (options.measureDrivenInitialPartition) {
                STORM_LOG_THROW(options.phiStates, storm::exceptions::InvalidOptionException, "Unable to compute measure-driven initial partition without phi states.");
                STORM_LOG_THROW(options.psiStates, storm::exceptions::InvalidOptionException, "Unable to compute measure-driven initial partition without psi states.");
                this->initializeMeasureDrivenPartition();
            } else {
                this->initializeLabelBasedPartition();
            }
            std::chrono::high_resolution_clock::duration initialPartitionTime = std::chrono::high_resolution_clock::now() - initialPartitionStart;
            
            this->initialize();
            
            std::chrono::high_resolution_clock::time_point refinementStart = std::chrono::high_resolution_clock::now();
            this->performPartitionRefinement();
            std::chrono::high_resolution_clock::duration refinementTime = std::chrono::high_resolution_clock::now() - refinementStart;
            
            std::chrono::high_resolution_clock::time_point extractionStart = std::chrono::high_resolution_clock::now();
            this->extractDecompositionBlocks();
            std::chrono::high_resolution_clock::duration extractionTime = std::chrono::high_resolution_clock::now() - extractionStart;
            
            std::chrono::high_resolution_clock::time_point quotientBuildStart = std::chrono::high_resolution_clock::now();
            if (options.buildQuotient) {
                this->buildQuotient();
            }
            std::chrono::high_resolution_clock::duration quotientBuildTime = std::chrono::high_resolution_clock::now() - quotientBuildStart;
            
            std::chrono::high_resolution_clock::duration totalTime = std::chrono::high_resolution_clock::now() - totalStart;
            
            if (storm::settings::getModule<storm::settings::modules::MarkovChainSettings>().isShowStatisticsSet()) {
                std::chrono::milliseconds initialPartitionTimeInMilliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(initialPartitionTime);
                std::chrono::milliseconds refinementTimeInMilliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(refinementTime);
                std::chrono::milliseconds extractionTimeInMilliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(extractionTime);
                std::chrono::milliseconds quotientBuildTimeInMilliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(quotientBuildTime);
                std::chrono::milliseconds totalTimeInMilliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(totalTime);
                std::cout << std::endl;
                std::cout << "Time breakdown:" << std::endl;
                std::cout << "    * time for initial partition: " << initialPartitionTimeInMilliseconds.count() << "ms" << std::endl;
                std::cout << "    * time for partitioning: " << refinementTimeInMilliseconds.count() << "ms" << std::endl;
                std::cout << "    * time for extraction: " << extractionTimeInMilliseconds.count() << "ms" << std::endl;
                std::cout << "    * time for building quotient: " << quotientBuildTimeInMilliseconds.count() << "ms" << std::endl;
                std::cout << "------------------------------------------" << std::endl;
                std::cout << "    * total time: " << totalTimeInMilliseconds.count() << "ms" << std::endl;
                std::cout << std::endl;
            }
        }
        
        template<typename ModelType, typename BlockDataType>
        void BisimulationDecomposition<ModelType, BlockDataType>::performPartitionRefinement() {
            // Insert all blocks into the splitter vector as a (potential) splitter.
            std::vector<Block<BlockDataType>*> splitterVector;
            std::for_each(partition.getBlocks().begin(), partition.getBlocks().end(), [&] (std::unique_ptr<Block<BlockDataType>> const& block) { block->data().setSplitter(); splitterVector.push_back(block.get()); } );
            
            // Then perform the actual splitting until there are no more splitters.
            uint_fast64_t iterations = 0;
            while (!splitterVector.empty()) {
                ++iterations;

                // Get and prepare the next splitter.
                // Sort the splitters according to their sizes to prefer small splitters. That is just a heuristic, but
                // tends to work well.
                std::sort(splitterVector.begin(), splitterVector.end(), [] (Block<BlockDataType> const* b1, Block<BlockDataType> const* b2) { return b1->getNumberOfStates() > b2->getNumberOfStates(); } );
                Block<BlockDataType>* splitter = splitterVector.back();
                splitterVector.pop_back();
                splitter->data().setSplitter(false);
                
                // Now refine the partition using the current splitter.
                refinePartitionBasedOnSplitter(*splitter, splitterVector);
            }
        }
        
        template<typename ModelType, typename BlockDataType>
        std::shared_ptr<ModelType> BisimulationDecomposition<ModelType, BlockDataType>::getQuotient() const {
            STORM_LOG_THROW(this->quotient != nullptr, storm::exceptions::IllegalFunctionCallException, "Unable to retrieve quotient model from bisimulation decomposition, because it was not built.");
            return this->quotient;
        }

        template<typename ModelType, typename BlockDataType>
        void BisimulationDecomposition<ModelType, BlockDataType>::splitInitialPartitionBasedOnStateRewards() {
            std::vector<ValueType> const& stateRewardVector = model.getUniqueRewardModel()->second.getStateRewardVector();
            partition.split([&stateRewardVector] (storm::storage::sparse::state_type const& a, storm::storage::sparse::state_type const& b) { return stateRewardVector[a] < stateRewardVector[b]; });
        }
        
        template<typename ModelType, typename BlockDataType>
        void BisimulationDecomposition<ModelType, BlockDataType>::initializeLabelBasedPartition() {
            partition = storm::storage::bisimulation::Partition<BlockDataType>(model.getNumberOfStates());

            for (auto const& label : options.respectedAtomicPropositions.get()) {
                if (label == "init") {
                    continue;
                }
                partition.splitStates(model.getStates(label));
            }
            
            // If the model has state rewards, we need to consider them, because otherwise reward properties are not
            // preserved.
            if (options.getKeepRewards() && model.hasRewardModel()) {
                this->splitInitialPartitionBasedOnStateRewards();
            }
        }
        
        template<typename ModelType, typename BlockDataType>
        void BisimulationDecomposition<ModelType, BlockDataType>::initializeMeasureDrivenPartition() {
            std::pair<storm::storage::BitVector, storm::storage::BitVector> statesWithProbability01 = this->getStatesWithProbability01();
            
            boost::optional<storm::storage::sparse::state_type> representativePsiState;
            if (!options.psiStates.get().empty()) {
                representativePsiState = *options.psiStates.get().begin();
            }
            
            partition = storm::storage::bisimulation::Partition<BlockDataType>(model.getNumberOfStates(), statesWithProbability01.first, options.getBounded() || options.getKeepRewards() ? options.psiStates.get() : statesWithProbability01.second, representativePsiState);
            
            // If the model has state rewards, we need to consider them, because otherwise reward properties are not
            // preserved.
            if (options.getKeepRewards() && model.hasRewardModel()) {
                this->splitInitialPartitionBasedOnStateRewards();
            }
        }
        
        template<typename ModelType, typename BlockDataType>
        void BisimulationDecomposition<ModelType, BlockDataType>::initialize() {
            // Intentionally left empty.
        }
        
        template<typename ModelType, typename BlockDataType>
        void BisimulationDecomposition<ModelType, BlockDataType>::extractDecompositionBlocks() {
            // Now move the states from the internal partition into their final place in the decomposition. We do so in
            // a way that maintains the block IDs as indices.
            this->blocks.resize(partition.size());
            for (auto const& blockPtr : partition.getBlocks()) {
                // We need to sort the states to allow for rapid construction of the blocks.
                partition.sortBlock(*blockPtr);
                
                // Convert the state-value-pairs to states only.
                this->blocks[blockPtr->getId()] = block_type(partition.begin(*blockPtr), partition.end(*blockPtr), true);
            }
        }
        
        template class BisimulationDecomposition<storm::models::sparse::Dtmc<double>, bisimulation::DeterministicBlockData>;
        template class BisimulationDecomposition<storm::models::sparse::Ctmc<double>, bisimulation::DeterministicBlockData>;
        template class BisimulationDecomposition<storm::models::sparse::Mdp<double>, bisimulation::DeterministicBlockData>;
        
#ifdef STORM_HAVE_CARL
        template class BisimulationDecomposition<storm::models::sparse::Dtmc<storm::RationalFunction>, bisimulation::DeterministicBlockData>;
        template class BisimulationDecomposition<storm::models::sparse::Ctmc<storm::RationalFunction>, bisimulation::DeterministicBlockData>;
        template class BisimulationDecomposition<storm::models::sparse::Mdp<storm::RationalFunction>, bisimulation::DeterministicBlockData>;
#endif
    }
}
