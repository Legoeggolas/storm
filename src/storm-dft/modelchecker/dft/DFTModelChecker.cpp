#include "DFTModelChecker.h"

#include "storm/settings/modules/IOSettings.h"
#include "storm/settings/modules/GeneralSettings.h"
#include "storm/builder/ParallelCompositionBuilder.h"
#include "storm/utility/bitoperations.h"
#include "storm/utility/DirectEncodingExporter.h"
#include "storm/modelchecker/results/ExplicitQuantitativeCheckResult.h"
#include "storm/modelchecker/results/ExplicitQualitativeCheckResult.h"
#include "storm/models/ModelType.h"

#include "storm-dft/builder/ExplicitDFTModelBuilder.h"
#include "storm-dft/storage/dft/DFTIsomorphism.h"
#include "storm-dft/settings/modules/FaultTreeSettings.h"


namespace storm {
    namespace modelchecker {

        template<typename ValueType>
        typename DFTModelChecker<ValueType>::dft_results
        DFTModelChecker<ValueType>::check(storm::storage::DFT<ValueType> const &origDft,
                                          std::vector<std::shared_ptr<const storm::logic::Formula>> const &properties,
                                          bool symred, bool allowModularisation, std::set<size_t> const &relevantEvents,
                                          bool allowDCForRelevantEvents, double approximationError,
                                          storm::builder::ApproximationHeuristic approximationHeuristic,
                                          bool eliminateChains, bool ignoreLabeling) {
            totalTimer.start();
            dft_results results;

            // Optimizing DFT
            storm::storage::DFT<ValueType> dft = origDft.optimize();

            // TODO: check that all paths reach the target state for approximation

            // Checking DFT
            // TODO: distinguish for all properties, not only for first one
            if (properties[0]->isTimeOperatorFormula() && allowModularisation) {
                // Use parallel composition as modularisation approach for expected time
                std::shared_ptr<storm::models::sparse::Model<ValueType>> model = buildModelViaComposition(dft,
                                                                                                          properties,
                                                                                                          symred, true,
                                                                                                          relevantEvents);
                // Model checking
                std::vector<ValueType> resultsValue = checkModel(model, properties);
                for (ValueType result : resultsValue) {
                    results.push_back(result);
                }
            } else {
                results = checkHelper(dft, properties, symred, allowModularisation, relevantEvents,
                                      allowDCForRelevantEvents, approximationError, approximationHeuristic,
                                      eliminateChains, ignoreLabeling);
            }
            totalTimer.stop();
            return results;
        }

        template<typename ValueType>
        typename DFTModelChecker<ValueType>::dft_results
        DFTModelChecker<ValueType>::checkHelper(storm::storage::DFT<ValueType> const &dft,
                                                property_vector const &properties, bool symred,
                                                bool allowModularisation, std::set<size_t> const &relevantEvents,
                                                bool allowDCForRelevantEvents, double approximationError,
                                                storm::builder::ApproximationHeuristic approximationHeuristic,
                                                bool eliminateChains, bool ignoreLabeling) {
            STORM_LOG_TRACE("Check helper called");
            std::vector<storm::storage::DFT<ValueType>> dfts;
            bool invResults = false;
            size_t nrK = 0; // K out of M
            size_t nrM = 0; // K out of M

            // Try modularisation
            if (allowModularisation) {
                switch (dft.topLevelType()) {
                    case storm::storage::DFTElementType::AND:
                        STORM_LOG_TRACE("top modularisation called AND");
                        dfts = dft.topModularisation();
                        STORM_LOG_TRACE("Modularisation into " << dfts.size() << " submodules.");
                        nrK = dfts.size();
                        nrM = dfts.size();
                        break;
                    case storm::storage::DFTElementType::OR:
                        STORM_LOG_TRACE("top modularisation called OR");
                        dfts = dft.topModularisation();
                        STORM_LOG_TRACE("Modularisation into " << dfts.size() << " submodules.");
                        nrK = 0;
                        nrM = dfts.size();
                        invResults = true;
                        break;
                    case storm::storage::DFTElementType::VOT:
                        STORM_LOG_TRACE("top modularisation called VOT");
                        dfts = dft.topModularisation();
                        STORM_LOG_TRACE("Modularisation into " << dfts.size() << " submodules.");
                        nrK = std::static_pointer_cast<storm::storage::DFTVot<ValueType> const>(
                                dft.getTopLevelGate())->threshold();
                        nrM = dfts.size();
                        if (nrK <= nrM / 2) {
                            nrK -= 1;
                            invResults = true;
                        }
                        break;
                    default:
                        // No static gate -> no modularisation applicable
                        break;
                }
            }

            // Perform modularisation
            if (dfts.size() > 1) {
                STORM_LOG_TRACE("Recursive CHECK Call");
                // TODO: compute simultaneously
                dft_results results;
                for (auto property : properties) {
                    if (!property->isProbabilityOperatorFormula()) {
                        STORM_LOG_WARN("Could not check property: " << *property);
                    } else {
                        // Recursively call model checking
                        std::vector<ValueType> res;
                        for (auto const ft : dfts) {
                            // TODO: allow approximation in modularisation
                            dft_results ftResults = checkHelper(ft, {property}, symred, true, relevantEvents,
                                                                allowDCForRelevantEvents, 0.0);
                            STORM_LOG_ASSERT(ftResults.size() == 1, "Wrong number of results");
                            res.push_back(boost::get<ValueType>(ftResults[0]));
                        }

                        // Combine modularisation results
                        STORM_LOG_TRACE("Combining all results... K=" << nrK << "; M=" << nrM << "; invResults="
                                                                      << (invResults ? "On" : "Off"));
                        ValueType result = storm::utility::zero<ValueType>();
                        int limK = invResults ? -1 : nrM + 1;
                        int chK = invResults ? -1 : 1;
                        // WARNING: there is a bug for computing permutations with more than 32 elements
                        STORM_LOG_THROW(res.size() < 32, storm::exceptions::NotSupportedException,
                                        "Permutations work only for < 32 elements");
                        for (int cK = nrK; cK != limK; cK += chK) {
                            STORM_LOG_ASSERT(cK >= 0, "ck negative.");
                            size_t permutation = smallestIntWithNBitsSet(static_cast<size_t>(cK));
                            do {
                                STORM_LOG_TRACE("Permutation=" << permutation);
                                ValueType permResult = storm::utility::one<ValueType>();
                                for (size_t i = 0; i < res.size(); ++i) {
                                    if (permutation & (1 << i)) {
                                        permResult *= res[i];
                                    } else {
                                        permResult *= storm::utility::one<ValueType>() - res[i];
                                    }
                                }
                                STORM_LOG_TRACE("Result for permutation:" << permResult);
                                permutation = nextBitPermutation(permutation);
                                result += permResult;
                            } while (permutation < (1 << nrM) && permutation != 0);
                        }
                        if (invResults) {
                            result = storm::utility::one<ValueType>() - result;
                        }
                        results.push_back(result);
                    }
                }
                return results;
            } else {
                // No modularisation was possible
                return checkDFT(dft, properties, symred, relevantEvents, allowDCForRelevantEvents, approximationError,
                                approximationHeuristic, eliminateChains, ignoreLabeling);
            }
        }

        template<typename ValueType>
        std::shared_ptr<storm::models::sparse::Ctmc<ValueType>>
        DFTModelChecker<ValueType>::buildModelViaComposition(storm::storage::DFT<ValueType> const &dft,
                                                             property_vector const &properties, bool symred,
                                                             bool allowModularisation,
                                                             std::set<size_t> const &relevantEvents,
                                                             bool allowDCForRelevantEvents) {
            // TODO: use approximation?
            STORM_LOG_TRACE("Build model via composition");
            std::vector<storm::storage::DFT<ValueType>> dfts;
            bool isAnd = true;

            // Try modularisation
            if (allowModularisation) {
                switch (dft.topLevelType()) {
                    case storm::storage::DFTElementType::AND:
                        STORM_LOG_TRACE("top modularisation called AND");
                        dfts = dft.topModularisation();
                        STORM_LOG_TRACE("Modularisation into " << dfts.size() << " submodules.");
                        isAnd = true;
                        break;
                    case storm::storage::DFTElementType::OR:
                        STORM_LOG_TRACE("top modularisation called OR");
                        dfts = dft.topModularisation();
                        STORM_LOG_TRACE("Modularsation into " << dfts.size() << " submodules.");
                        isAnd = false;
                        break;
                    case storm::storage::DFTElementType::VOT:
                        // TODO enable modularisation for voting gate
                        break;
                    default:
                        // No static gate -> no modularisation applicable
                        break;
                }
            }

            // Perform modularisation via parallel composition
            if (dfts.size() > 1) {
                STORM_LOG_TRACE("Recursive CHECK Call");
                bool firstTime = true;
                std::shared_ptr<storm::models::sparse::Ctmc<ValueType>> composedModel;
                for (auto const ft : dfts) {
                    STORM_LOG_DEBUG("Building Model via parallel composition...");
                    explorationTimer.start();

                    // Find symmetries
                    std::map<size_t, std::vector<std::vector<size_t>>> emptySymmetry;
                    storm::storage::DFTIndependentSymmetries symmetries(emptySymmetry);
                    if (symred) {
                        auto colouring = ft.colourDFT();
                        symmetries = ft.findSymmetries(colouring);
                        STORM_LOG_DEBUG("Found " << symmetries.groups.size() << " symmetries.");
                        STORM_LOG_TRACE("Symmetries: " << std::endl << symmetries);
                    }

                    // Build a single CTMC
                    STORM_LOG_DEBUG("Building Model from DFT with top level element " << ft.getElement(ft.getTopLevelIndex())->toString() << " ...");
                    storm::builder::ExplicitDFTModelBuilder<ValueType> builder(ft, symmetries, relevantEvents, allowDCForRelevantEvents);
                    builder.buildModel(0, 0.0);
                    std::shared_ptr<storm::models::sparse::Model<ValueType>> model = builder.getModel();
                    explorationTimer.stop();

                    STORM_LOG_THROW(model->isOfType(storm::models::ModelType::Ctmc),
                                    storm::exceptions::NotSupportedException,
                                    "Parallel composition only applicable for CTMCs");
                    std::shared_ptr<storm::models::sparse::Ctmc<ValueType>> ctmc = model->template as<storm::models::sparse::Ctmc<ValueType>>();

                    // Apply bisimulation to new CTMC
                    bisimulationTimer.start();
                    ctmc = storm::api::performDeterministicSparseBisimulationMinimization<storm::models::sparse::Ctmc<ValueType>>(
                            ctmc, properties,
                            storm::storage::BisimulationType::Weak)->template as<storm::models::sparse::Ctmc<ValueType>>();
                    bisimulationTimer.stop();

                    if (firstTime) {
                        composedModel = ctmc;
                        firstTime = false;
                    } else {
                        composedModel = storm::builder::ParallelCompositionBuilder<ValueType>::compose(composedModel,
                                                                                                       ctmc, isAnd);
                    }

                    // Apply bisimulation to parallel composition
                    bisimulationTimer.start();
                    composedModel = storm::api::performDeterministicSparseBisimulationMinimization<storm::models::sparse::Ctmc<ValueType>>(
                            composedModel, properties,
                            storm::storage::BisimulationType::Weak)->template as<storm::models::sparse::Ctmc<ValueType>>();
                    bisimulationTimer.stop();

                    STORM_LOG_DEBUG("No. states (Composed): " << composedModel->getNumberOfStates());
                    STORM_LOG_DEBUG("No. transitions (Composed): " << composedModel->getNumberOfTransitions());
                    if (composedModel->getNumberOfStates() <= 15) {
                        STORM_LOG_TRACE("Transition matrix: " << std::endl << composedModel->getTransitionMatrix());
                    } else {
                        STORM_LOG_TRACE("Transition matrix: too big to print");
                    }

                }
                //composedModel->printModelInformationToStream(std::cout);
                return composedModel;
            } else {
                // No composition was possible
                explorationTimer.start();
                // Find symmetries
                std::map<size_t, std::vector<std::vector<size_t>>> emptySymmetry;
                storm::storage::DFTIndependentSymmetries symmetries(emptySymmetry);
                if (symred) {
                    auto colouring = dft.colourDFT();
                    symmetries = dft.findSymmetries(colouring);
                    STORM_LOG_DEBUG("Found " << symmetries.groups.size() << " symmetries.");
                    STORM_LOG_TRACE("Symmetries: " << std::endl << symmetries);
                }
                // Build a single CTMC
                STORM_LOG_DEBUG("Building Model...");

                storm::builder::ExplicitDFTModelBuilder<ValueType> builder(dft, symmetries, relevantEvents,
                                                                           allowDCForRelevantEvents);
                builder.buildModel(0, 0.0);
                std::shared_ptr<storm::models::sparse::Model<ValueType>> model = builder.getModel();
                //model->printModelInformationToStream(std::cout);
                explorationTimer.stop();
                STORM_LOG_THROW(model->isOfType(storm::models::ModelType::Ctmc),
                                storm::exceptions::NotSupportedException,
                                "Parallel composition only applicable for CTMCs");
                return model->template as<storm::models::sparse::Ctmc<ValueType>>();
            }
        }

        template<typename ValueType>
        typename DFTModelChecker<ValueType>::dft_results
        DFTModelChecker<ValueType>::checkDFT(storm::storage::DFT<ValueType> const &dft,
                                             property_vector const &properties, bool symred,
                                             std::set<size_t> const &relevantEvents, bool allowDCForRelevantEvents,
                                             double approximationError,
                                             storm::builder::ApproximationHeuristic approximationHeuristic,
                                             bool eliminateChains, bool ignoreLabeling) {
            explorationTimer.start();

            // Find symmetries
            std::map<size_t, std::vector<std::vector<size_t>>> emptySymmetry;
            storm::storage::DFTIndependentSymmetries symmetries(emptySymmetry);
            if (symred) {
                auto colouring = dft.colourDFT();
                symmetries = dft.findSymmetries(colouring);
                STORM_LOG_DEBUG("Found " << symmetries.groups.size() << " symmetries.");
                STORM_LOG_TRACE("Symmetries: " << std::endl << symmetries);
            }

            if (approximationError > 0.0) {
                // Comparator for checking the error of the approximation
                storm::utility::ConstantsComparator<ValueType> comparator;
                // Build approximate Markov Automata for lower and upper bound
                approximation_result approxResult = std::make_pair(storm::utility::zero<ValueType>(),
                                                                   storm::utility::zero<ValueType>());
                std::shared_ptr<storm::models::sparse::Model<ValueType>> model;
                std::vector<ValueType> newResult;
                storm::builder::ExplicitDFTModelBuilder<ValueType> builder(dft, symmetries, relevantEvents,
                                                                           allowDCForRelevantEvents);

                // TODO: compute approximation for all properties simultaneously?
                std::shared_ptr<const storm::logic::Formula> property = properties[0];
                if (properties.size() > 1) {
                    STORM_LOG_WARN("Computing approximation only for first property: " << *property);
                }

                bool probabilityFormula = property->isProbabilityOperatorFormula();
                STORM_LOG_ASSERT((property->isTimeOperatorFormula() && !probabilityFormula) ||
                                 (!property->isTimeOperatorFormula() && probabilityFormula),
                                 "Probability formula not initialized correctly");
                size_t iteration = 0;
                do {
                    // Iteratively build finer models
                    if (iteration > 0) {
                        explorationTimer.start();
                    }
                    STORM_LOG_DEBUG("Building model...");
                    // TODO refine model using existing model and MC results
                    builder.buildModel(iteration, approximationError, approximationHeuristic);
                    explorationTimer.stop();
                    buildingTimer.start();

                    // TODO: possible to do bisimulation on approximated model and not on concrete one?

                    // Build model for lower bound
                    STORM_LOG_DEBUG("Getting model for lower bound...");
                    model = builder.getModelApproximation(true, !probabilityFormula);
                    // We only output the info from the lower bound as the info for the upper bound is the same
                    //model->printModelInformationToStream(std::cout);
                    buildingTimer.stop();

                    // Check lower bounds
                    newResult = checkModel(model, {property});
                    STORM_LOG_ASSERT(newResult.size() == 1, "Wrong size for result vector.");
                    STORM_LOG_ASSERT(iteration == 0 || !comparator.isLess(newResult[0], approxResult.first),
                                     "New under-approximation " << newResult[0] << " is smaller than old result "
                                                                << approxResult.first);
                    approxResult.first = newResult[0];

                    // Build model for upper bound
                    STORM_LOG_DEBUG("Getting model for upper bound...");
                    buildingTimer.start();
                    model = builder.getModelApproximation(false, !probabilityFormula);
                    buildingTimer.stop();
                    // Check upper bound
                    newResult = checkModel(model, {property});
                    STORM_LOG_ASSERT(newResult.size() == 1, "Wrong size for result vector.");
                    STORM_LOG_ASSERT(iteration == 0 || !comparator.isLess(approxResult.second, newResult[0]),
                                     "New over-approximation " << newResult[0] << " is greater than old result "
                                                               << approxResult.second);
                    approxResult.second = newResult[0];

                    ++iteration;
                    STORM_LOG_ASSERT(comparator.isLess(approxResult.first, approxResult.second) ||
                                     comparator.isEqual(approxResult.first, approxResult.second),
                                     "Under-approximation " << approxResult.first
                                                            << " is greater than over-approximation "
                                                            << approxResult.second);
                    //STORM_LOG_INFO("Result after iteration " << iteration << ": (" << std::setprecision(10) << approxResult.first << ", " << approxResult.second << ")");
                    totalTimer.stop();
                    printTimings();
                    totalTimer.start();
                    STORM_LOG_THROW(!storm::utility::isInfinity<ValueType>(approxResult.first) &&
                                    !storm::utility::isInfinity<ValueType>(approxResult.second),
                                    storm::exceptions::NotSupportedException,
                                    "Approximation does not work if result might be infinity.");
                } while (!isApproximationSufficient(approxResult.first, approxResult.second, approximationError,
                                                    probabilityFormula));

                //STORM_LOG_INFO("Finished approximation after " << iteration << " iteration" << (iteration > 1 ? "s." : "."));
                dft_results results;
                results.push_back(approxResult);
                return results;
            } else {
                // Build a single Markov Automaton
                auto ioSettings = storm::settings::getModule<storm::settings::modules::IOSettings>();
                STORM_LOG_DEBUG("Building Model...");
                storm::builder::ExplicitDFTModelBuilder<ValueType> builder(dft, symmetries, relevantEvents,
                                                                           allowDCForRelevantEvents);
                builder.buildModel(0, 0.0);
                std::shared_ptr<storm::models::sparse::Model<ValueType>> model = builder.getModel();
                if (eliminateChains && model->isOfType(storm::models::ModelType::MarkovAutomaton)) {
                    auto ma = std::static_pointer_cast<storm::models::sparse::MarkovAutomaton<ValueType>>(model);
                    model = storm::transformer::NonMarkovianChainTransformer<ValueType>::eliminateNonmarkovianStates(ma,
                                                                                                                     !ignoreLabeling);
                }
                explorationTimer.stop();

                // Print model information
                if (printInfo) {
                    model->printModelInformationToStream(std::cout);
                }

                // Export the model if required
                // TODO move this outside of the model checker?
                if (ioSettings.isExportExplicitSet()) {
                    std::vector<std::string> parameterNames;
                    // TODO fill parameter names
                    storm::api::exportSparseModelAsDrn(model, ioSettings.getExportExplicitFilename(), parameterNames);
                }
                if (ioSettings.isExportDotSet()) {
                    std::ofstream stream;
                    storm::utility::openFile(ioSettings.getExportDotFilename(), stream);
                    model->writeDotToStream(stream, true, true);
                    storm::utility::closeFile(stream);
                }

                // Model checking
                std::vector<ValueType> resultsValue = checkModel(model, properties);
                dft_results results;
                for (ValueType result : resultsValue) {
                    results.push_back(result);
                }
                return results;
            }
        }

        template<typename ValueType>
        std::vector<ValueType>
        DFTModelChecker<ValueType>::checkModel(std::shared_ptr<storm::models::sparse::Model<ValueType>> &model,
                                               property_vector const &properties) {
            // Bisimulation
            if (model->isOfType(storm::models::ModelType::Ctmc) &&
                storm::settings::getModule<storm::settings::modules::GeneralSettings>().isBisimulationSet()) {
                bisimulationTimer.start();
                STORM_LOG_DEBUG("Bisimulation...");
                model = storm::api::performDeterministicSparseBisimulationMinimization<storm::models::sparse::Ctmc<ValueType>>(
                        model->template as<storm::models::sparse::Ctmc<ValueType>>(), properties,
                        storm::storage::BisimulationType::Weak)->template as<storm::models::sparse::Ctmc<ValueType>>();
                STORM_LOG_DEBUG("No. states (Bisimulation): " << model->getNumberOfStates());
                STORM_LOG_DEBUG("No. transitions (Bisimulation): " << model->getNumberOfTransitions());
                bisimulationTimer.stop();
            }

            // Check the model
            STORM_LOG_DEBUG("Model checking...");
            modelCheckingTimer.start();
            std::vector<ValueType> results;

            // Check each property
            storm::utility::Stopwatch singleModelCheckingTimer;
            for (auto property : properties) {
                singleModelCheckingTimer.reset();
                singleModelCheckingTimer.start();
                //STORM_PRINT_AND_LOG("Model checking property " << *property << " ..." << std::endl);
                std::unique_ptr<storm::modelchecker::CheckResult> result(
                        storm::api::verifyWithSparseEngine<ValueType>(model, storm::api::createTask<ValueType>(property,
                                                                                                               true)));
                STORM_LOG_ASSERT(result, "Result does not exist.");
                result->filter(storm::modelchecker::ExplicitQualitativeCheckResult(model->getInitialStates()));
                ValueType resultValue = result->asExplicitQuantitativeCheckResult<ValueType>().getValueMap().begin()->second;
                //STORM_PRINT_AND_LOG("Result (initial states): " << resultValue << std::endl);
                results.push_back(resultValue);
                singleModelCheckingTimer.stop();
                //STORM_PRINT_AND_LOG("Time for model checking: " << singleModelCheckingTimer << "." << std::endl);
            }
            modelCheckingTimer.stop();
            STORM_LOG_DEBUG("Model checking done.");
            return results;
        }

        template<typename ValueType>
        bool DFTModelChecker<ValueType>::isApproximationSufficient(ValueType, ValueType, double, bool) {
            STORM_LOG_THROW(false, storm::exceptions::NotImplementedException, "Approximation works only for double.");
        }

        template<>
        bool DFTModelChecker<double>::isApproximationSufficient(double lowerBound, double upperBound,
                                                                double approximationError, bool relative) {
            STORM_LOG_THROW(!std::isnan(lowerBound) && !std::isnan(upperBound),
                            storm::exceptions::NotSupportedException, "Approximation does not work if result is NaN.");
            if (relative) {
                return upperBound - lowerBound <= approximationError;
            } else {
                return upperBound - lowerBound <= approximationError * (lowerBound + upperBound) / 2;
            }
        }

        template<typename ValueType>
        void DFTModelChecker<ValueType>::printTimings(std::ostream &os) {
            os << "Times:" << std::endl;
            os << "Exploration:\t" << explorationTimer << std::endl;
            os << "Building:\t" << buildingTimer << std::endl;
            os << "Bisimulation:\t" << bisimulationTimer << std::endl;
            os << "Modelchecking:\t" << modelCheckingTimer << std::endl;
            os << "Total:\t\t" << totalTimer << std::endl;
        }

        template<typename ValueType>
        void DFTModelChecker<ValueType>::printResults(dft_results const &results, std::ostream &os) {
            bool first = true;
            os << "Result: [";
            for (auto result : results) {
                if (first) {
                    first = false;
                } else {
                    os << ", ";
                }
                os << boost::apply_visitor(ResultOutputVisitor(), result);
            }
            os << "]" << std::endl;
        }


        template
        class DFTModelChecker<double>;

#ifdef STORM_HAVE_CARL

        template
        class DFTModelChecker<storm::RationalFunction>;

#endif
    }
}
