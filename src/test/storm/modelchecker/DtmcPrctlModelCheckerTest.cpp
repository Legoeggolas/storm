#include "gtest/gtest.h"
#include "test/storm_gtest.h"
#include "storm-config.h"

#include "storm/api/builder.h"
#include "storm/api/model_descriptions.h"
#include "storm/api/properties.h"
#include "storm/parser/FormulaParser.h"
#include "storm/logic/Formulas.h"
#include "storm/solver/EigenLinearEquationSolver.h"
#include "storm/models/sparse/StandardRewardModel.h"
#include "storm/models/symbolic/StandardRewardModel.h"
#include "storm/models/sparse/Dtmc.h"
#include "storm/models/symbolic/Dtmc.h"
#include "storm/modelchecker/prctl/SparseDtmcPrctlModelChecker.h"
#include "storm/modelchecker/prctl/HybridDtmcPrctlModelChecker.h"
#include "storm/modelchecker/prctl/SymbolicDtmcPrctlModelChecker.h"
#include "storm/modelchecker/results/QuantitativeCheckResult.h"
#include "storm/modelchecker/results/ExplicitQualitativeCheckResult.h"
#include "storm/modelchecker/results/SymbolicQualitativeCheckResult.h"
#include "storm/modelchecker/results/QualitativeCheckResult.h"
#include "storm/parser/PrismParser.h"
#include "storm/storage/expressions/ExpressionManager.h"
#include "storm/settings/modules/CoreSettings.h"
#include "storm/environment/solver/NativeSolverEnvironment.h"
#include "storm/environment/solver/GmmxxSolverEnvironment.h"
#include "storm/environment/solver/EigenSolverEnvironment.h"

namespace {
    
    class SparseGmmxxGmresIluEnvironment {
    public:
        static const storm::dd::DdType ddType = storm::dd::DdType::Sylvan; // unused for sparse models
        static const storm::settings::modules::CoreSettings::Engine engine = storm::settings::modules::CoreSettings::Engine::Sparse;
        static const bool isExact = false;
        typedef double ValueType;
        typedef storm::models::sparse::Dtmc<ValueType> ModelType;
        static storm::Environment createEnvironment() {
            storm::Environment env;
            env.solver().setLinearEquationSolverType(storm::solver::EquationSolverType::Gmmxx);
            env.solver().gmmxx().setMethod(storm::solver::GmmxxLinearEquationSolverMethod::Gmres);
            env.solver().gmmxx().setPreconditioner(storm::solver::GmmxxLinearEquationSolverPreconditioner::Ilu);
            env.solver().gmmxx().setPrecision(storm::utility::convertNumber<storm::RationalNumber>(1e-8));
            return env;
        }
    };
    
    class SparseGmmxxGmresDiagEnvironment {
    public:
        static const storm::dd::DdType ddType = storm::dd::DdType::Sylvan; // unused for sparse models
        static const storm::settings::modules::CoreSettings::Engine engine = storm::settings::modules::CoreSettings::Engine::Sparse;
        static const bool isExact = false;
        typedef double ValueType;
        typedef storm::models::sparse::Dtmc<ValueType> ModelType;
        static storm::Environment createEnvironment() {
            storm::Environment env;
            env.solver().setLinearEquationSolverType(storm::solver::EquationSolverType::Gmmxx);
            env.solver().gmmxx().setMethod(storm::solver::GmmxxLinearEquationSolverMethod::Gmres);
            env.solver().gmmxx().setPreconditioner(storm::solver::GmmxxLinearEquationSolverPreconditioner::Diagonal);
            env.solver().gmmxx().setPrecision(storm::utility::convertNumber<storm::RationalNumber>(1e-8));
            return env;
        }
    };
    
    class SparseGmmxxBicgstabIluEnvironment {
    public:
        static const storm::dd::DdType ddType = storm::dd::DdType::Sylvan; // unused for sparse models
        static const storm::settings::modules::CoreSettings::Engine engine = storm::settings::modules::CoreSettings::Engine::Sparse;
        static const bool isExact = false;
        typedef double ValueType;
        typedef storm::models::sparse::Dtmc<ValueType> ModelType;
        static storm::Environment createEnvironment() {
            storm::Environment env;
            env.solver().setLinearEquationSolverType(storm::solver::EquationSolverType::Gmmxx);
            env.solver().gmmxx().setMethod(storm::solver::GmmxxLinearEquationSolverMethod::Bicgstab);
            env.solver().gmmxx().setPreconditioner(storm::solver::GmmxxLinearEquationSolverPreconditioner::Ilu);
            env.solver().gmmxx().setPrecision(storm::utility::convertNumber<storm::RationalNumber>(1e-8));
            return env;
        }
    };
    
    class SparseEigenDGmresEnvironment {
    public:
        static const storm::dd::DdType ddType = storm::dd::DdType::Sylvan; // unused for sparse models
        static const storm::settings::modules::CoreSettings::Engine engine = storm::settings::modules::CoreSettings::Engine::Sparse;
        static const bool isExact = false;
        typedef double ValueType;
        typedef storm::models::sparse::Dtmc<ValueType> ModelType;
        static storm::Environment createEnvironment() {
            storm::Environment env;
            env.solver().setLinearEquationSolverType(storm::solver::EquationSolverType::Eigen);
            env.solver().eigen().setMethod(storm::solver::EigenLinearEquationSolverMethod::DGmres);
            env.solver().eigen().setPreconditioner(storm::solver::EigenLinearEquationSolverPreconditioner::Ilu);
            env.solver().eigen().setPrecision(storm::utility::convertNumber<storm::RationalNumber>(1e-8));
            return env;
        }
    };
    
    class SparseEigenDoubleLUEnvironment {
    public:
        static const storm::dd::DdType ddType = storm::dd::DdType::Sylvan; // unused for sparse models
        static const storm::settings::modules::CoreSettings::Engine engine = storm::settings::modules::CoreSettings::Engine::Sparse;
        static const bool isExact = false;
        typedef double ValueType;
        typedef storm::models::sparse::Dtmc<ValueType> ModelType;
        static storm::Environment createEnvironment() {
            storm::Environment env;
            env.solver().setLinearEquationSolverType(storm::solver::EquationSolverType::Eigen);
            env.solver().eigen().setMethod(storm::solver::EigenLinearEquationSolverMethod::SparseLU);
            return env;
        }
    };
    
    class SparseEigenRationalLUEnvironment {
    public:
        static const storm::dd::DdType ddType = storm::dd::DdType::Sylvan; // unused for sparse models
        static const storm::settings::modules::CoreSettings::Engine engine = storm::settings::modules::CoreSettings::Engine::Sparse;
        static const bool isExact = true;
        typedef storm::RationalNumber ValueType;
        typedef storm::models::sparse::Dtmc<ValueType> ModelType;
        static storm::Environment createEnvironment() {
            storm::Environment env;
            env.solver().setLinearEquationSolverType(storm::solver::EquationSolverType::Eigen);
            env.solver().eigen().setMethod(storm::solver::EigenLinearEquationSolverMethod::SparseLU);
            return env;
        }
    };
    
    class SparseRationalEliminationEnvironment {
    public:
        static const storm::dd::DdType ddType = storm::dd::DdType::Sylvan; // unused for sparse models
        static const storm::settings::modules::CoreSettings::Engine engine = storm::settings::modules::CoreSettings::Engine::Sparse;
        static const bool isExact = true;
        typedef storm::RationalNumber ValueType;
        typedef storm::models::sparse::Dtmc<ValueType> ModelType;
        static storm::Environment createEnvironment() {
            storm::Environment env;
            env.solver().setLinearEquationSolverType(storm::solver::EquationSolverType::Elimination);
            return env;
        }
    };
    
    class SparseNativeJacobiEnvironment {
    public:
        static const storm::dd::DdType ddType = storm::dd::DdType::Sylvan; // unused for sparse models
        static const storm::settings::modules::CoreSettings::Engine engine = storm::settings::modules::CoreSettings::Engine::Sparse;
        static const bool isExact = false;
        typedef double ValueType;
        typedef storm::models::sparse::Dtmc<ValueType> ModelType;
        static storm::Environment createEnvironment() {
            storm::Environment env;
            env.solver().setLinearEquationSolverType(storm::solver::EquationSolverType::Native);
            env.solver().native().setMethod(storm::solver::NativeLinearEquationSolverMethod::Jacobi);
            env.solver().native().setPrecision(storm::utility::convertNumber<storm::RationalNumber>(1e-8));
            return env;
        }
    };

    class SparseNativeWalkerChaeEnvironment {
    public:
        static const storm::dd::DdType ddType = storm::dd::DdType::Sylvan; // unused for sparse models
        static const storm::settings::modules::CoreSettings::Engine engine = storm::settings::modules::CoreSettings::Engine::Sparse;
        static const bool isExact = false;
        typedef double ValueType;
        typedef storm::models::sparse::Dtmc<ValueType> ModelType;
        static storm::Environment createEnvironment() {
            storm::Environment env;
            env.solver().setLinearEquationSolverType(storm::solver::EquationSolverType::Native);
            env.solver().native().setMethod(storm::solver::NativeLinearEquationSolverMethod::WalkerChae);
            env.solver().native().setMaximalNumberOfIterations(1000000);
            env.solver().native().setPrecision(storm::utility::convertNumber<storm::RationalNumber>(1e-7));
            return env;
        }
    };

    class SparseNativeSorEnvironment {
    public:
        static const storm::dd::DdType ddType = storm::dd::DdType::Sylvan; // unused for sparse models
        static const storm::settings::modules::CoreSettings::Engine engine = storm::settings::modules::CoreSettings::Engine::Sparse;
        static const bool isExact = false;
        typedef double ValueType;
        typedef storm::models::sparse::Dtmc<ValueType> ModelType;
        static storm::Environment createEnvironment() {
            storm::Environment env;
            env.solver().setLinearEquationSolverType(storm::solver::EquationSolverType::Native);
            env.solver().native().setMethod(storm::solver::NativeLinearEquationSolverMethod::SOR);
            env.solver().native().setPrecision(storm::utility::convertNumber<storm::RationalNumber>(1e-8));
            return env;
        }
    };

    class SparseNativePowerEnvironment {
    public:
        static const storm::dd::DdType ddType = storm::dd::DdType::Sylvan; // unused for sparse models
        static const storm::settings::modules::CoreSettings::Engine engine = storm::settings::modules::CoreSettings::Engine::Sparse;
        static const bool isExact = false;
        typedef double ValueType;
        typedef storm::models::sparse::Dtmc<ValueType> ModelType;
        static storm::Environment createEnvironment() {
            storm::Environment env;
            env.solver().setLinearEquationSolverType(storm::solver::EquationSolverType::Native);
            env.solver().native().setMethod(storm::solver::NativeLinearEquationSolverMethod::Power);
            env.solver().native().setPrecision(storm::utility::convertNumber<storm::RationalNumber>(1e-8));
            return env;
        }
    };

    class SparseNativeSoundPowerEnvironment {
    public:
        static const storm::dd::DdType ddType = storm::dd::DdType::Sylvan; // unused for sparse models
        static const storm::settings::modules::CoreSettings::Engine engine = storm::settings::modules::CoreSettings::Engine::Sparse;
        static const bool isExact = false;
        typedef double ValueType;
        typedef storm::models::sparse::Dtmc<ValueType> ModelType;
        static storm::Environment createEnvironment() {
            storm::Environment env;
            env.solver().setForceSoundness(true);
            env.solver().setLinearEquationSolverType(storm::solver::EquationSolverType::Native);
            env.solver().native().setMethod(storm::solver::NativeLinearEquationSolverMethod::Power);
            env.solver().native().setPrecision(storm::utility::convertNumber<storm::RationalNumber>(1e-6));
            return env;
        }
    };

    class SparseNativeRationalSearchEnvironment {
    public:
        static const storm::dd::DdType ddType = storm::dd::DdType::Sylvan; // unused for sparse models
        static const storm::settings::modules::CoreSettings::Engine engine = storm::settings::modules::CoreSettings::Engine::Sparse;
        static const bool isExact = true;
        typedef storm::RationalNumber ValueType;
        typedef storm::models::sparse::Dtmc<ValueType> ModelType;
        static storm::Environment createEnvironment() {
            storm::Environment env;
            env.solver().setLinearEquationSolverType(storm::solver::EquationSolverType::Native);
            env.solver().native().setMethod(storm::solver::NativeLinearEquationSolverMethod::RationalSearch);
            return env;
        }
    };

    class HybridSylvanGmmxxGmresEnvironment {
    public:
        static const storm::dd::DdType ddType = storm::dd::DdType::Sylvan;
        static const storm::settings::modules::CoreSettings::Engine engine = storm::settings::modules::CoreSettings::Engine::Hybrid;
        static const bool isExact = false;
        typedef double ValueType;
        typedef storm::models::symbolic::Dtmc<ddType, ValueType> ModelType;
        static storm::Environment createEnvironment() {
            storm::Environment env;
            env.solver().setLinearEquationSolverType(storm::solver::EquationSolverType::Gmmxx);
            env.solver().gmmxx().setMethod(storm::solver::GmmxxLinearEquationSolverMethod::Gmres);
            env.solver().gmmxx().setPrecision(storm::utility::convertNumber<storm::RationalNumber>(1e-8));
            return env;
        }
    };
    
    class HybridCuddNativeJacobiEnvironment {
    public:
        static const storm::dd::DdType ddType = storm::dd::DdType::CUDD;
        static const storm::settings::modules::CoreSettings::Engine engine = storm::settings::modules::CoreSettings::Engine::Hybrid;
        static const bool isExact = false;
        typedef double ValueType;
        typedef storm::models::symbolic::Dtmc<ddType, ValueType> ModelType;
        static storm::Environment createEnvironment() {
            storm::Environment env;
            env.solver().setLinearEquationSolverType(storm::solver::EquationSolverType::Native);
            env.solver().native().setMethod(storm::solver::NativeLinearEquationSolverMethod::Jacobi);
            env.solver().native().setPrecision(storm::utility::convertNumber<storm::RationalNumber>(1e-8));
            return env;
        }
    };
    
    class HybridCuddNativeSoundPowerEnvironment {
    public:
        static const storm::dd::DdType ddType = storm::dd::DdType::CUDD;
        static const storm::settings::modules::CoreSettings::Engine engine = storm::settings::modules::CoreSettings::Engine::Hybrid;
        static const bool isExact = false;
        typedef double ValueType;
        typedef storm::models::symbolic::Dtmc<ddType, ValueType> ModelType;
        static storm::Environment createEnvironment() {
            storm::Environment env;
            env.solver().setForceSoundness(true);
            env.solver().setLinearEquationSolverType(storm::solver::EquationSolverType::Native);
            env.solver().native().setMethod(storm::solver::NativeLinearEquationSolverMethod::Power);
            env.solver().native().setPrecision(storm::utility::convertNumber<storm::RationalNumber>(1e-6));
            return env;
        }
    };
    
    class HybridSylvanNativeRationalSearchEnvironment {
    public:
        static const storm::dd::DdType ddType = storm::dd::DdType::Sylvan;
        static const storm::settings::modules::CoreSettings::Engine engine = storm::settings::modules::CoreSettings::Engine::Hybrid;
        static const bool isExact = true;
        typedef storm::RationalNumber ValueType;
        typedef storm::models::symbolic::Dtmc<ddType, ValueType> ModelType;
        static storm::Environment createEnvironment() {
            storm::Environment env;
            env.solver().setLinearEquationSolverType(storm::solver::EquationSolverType::Native);
            env.solver().native().setMethod(storm::solver::NativeLinearEquationSolverMethod::RationalSearch);
            return env;
        }
    };
    
    class DdSylvanNativePowerEnvironment {
    public:
        static const storm::dd::DdType ddType = storm::dd::DdType::Sylvan;
        static const storm::settings::modules::CoreSettings::Engine engine = storm::settings::modules::CoreSettings::Engine::Dd;
        static const bool isExact = false;
        typedef double ValueType;
        typedef storm::models::symbolic::Dtmc<ddType, ValueType> ModelType;
        static storm::Environment createEnvironment() {
            storm::Environment env;
            env.solver().setLinearEquationSolverType(storm::solver::EquationSolverType::Native);
            env.solver().native().setMethod(storm::solver::NativeLinearEquationSolverMethod::Power);
            env.solver().native().setPrecision(storm::utility::convertNumber<storm::RationalNumber>(1e-8));
            return env;
        }
    };

    class DdCuddNativeJacobiEnvironment {
    public:
        static const storm::dd::DdType ddType = storm::dd::DdType::CUDD;
        static const storm::settings::modules::CoreSettings::Engine engine = storm::settings::modules::CoreSettings::Engine::Dd;
        static const bool isExact = false;
        typedef double ValueType;
        typedef storm::models::symbolic::Dtmc<ddType, ValueType> ModelType;
        static storm::Environment createEnvironment() {
            storm::Environment env;
            env.solver().setLinearEquationSolverType(storm::solver::EquationSolverType::Native);
            env.solver().native().setMethod(storm::solver::NativeLinearEquationSolverMethod::Power);
            env.solver().native().setPrecision(storm::utility::convertNumber<storm::RationalNumber>(1e-8));
            return env;
        }
    };

    class DdSylvanRationalSearchEnvironment {
    public:
        static const storm::dd::DdType ddType = storm::dd::DdType::Sylvan;
        static const storm::settings::modules::CoreSettings::Engine engine = storm::settings::modules::CoreSettings::Engine::Dd;
        static const bool isExact = true;
        typedef storm::RationalNumber ValueType;
        typedef storm::models::symbolic::Dtmc<ddType, ValueType> ModelType;
        static storm::Environment createEnvironment() {
            storm::Environment env;
            env.solver().setLinearEquationSolverType(storm::solver::EquationSolverType::Native);
            env.solver().native().setMethod(storm::solver::NativeLinearEquationSolverMethod::RationalSearch);
            return env;
        }
    };

    template<typename TestType>
    class DtmcPrctlModelCheckerTest : public ::testing::Test {
    public:
        typedef typename TestType::ValueType ValueType;
        typedef typename storm::models::sparse::Dtmc<ValueType> SparseModelType;
        typedef typename storm::models::symbolic::Dtmc<TestType::ddType, ValueType> SymbolicModelType;
        
        DtmcPrctlModelCheckerTest() : _environment(TestType::createEnvironment()) {}
        storm::Environment const& env() const { return _environment; }
        ValueType parseNumber(std::string const& input) const { return storm::utility::convertNumber<ValueType>(input);}
        ValueType precision() const { return TestType::isExact ? parseNumber("0") : parseNumber("1e-6");}
        bool isSparseModel() const { return std::is_same<typename TestType::ModelType, SparseModelType>::value; }
        bool isSymbolicModel() const { return std::is_same<typename TestType::ModelType, SymbolicModelType>::value; }
        
        template <typename MT = typename TestType::ModelType>
        typename std::enable_if<std::is_same<MT, SparseModelType>::value, std::pair<std::shared_ptr<MT>, std::vector<std::shared_ptr<storm::logic::Formula const>>>>::type
        buildModelFormulas(std::string const& pathToPrismFile, std::string const& formulasAsString, std::string const& constantDefinitionString = "") const {
            std::pair<std::shared_ptr<MT>, std::vector<std::shared_ptr<storm::logic::Formula const>>> result;
            storm::prism::Program program = storm::api::parseProgram(pathToPrismFile);
            program = storm::utility::prism::preprocess(program, constantDefinitionString);
            result.second = storm::api::extractFormulasFromProperties(storm::api::parsePropertiesForPrismProgram(formulasAsString, program));
            result.first = storm::api::buildSparseModel<ValueType>(program, result.second)->template as<MT>();
            return result;
        }
        
        template <typename MT = typename TestType::ModelType>
        typename std::enable_if<std::is_same<MT, SymbolicModelType>::value, std::pair<std::shared_ptr<MT>, std::vector<std::shared_ptr<storm::logic::Formula const>>>>::type
        buildModelFormulas(std::string const& pathToPrismFile, std::string const& formulasAsString, std::string const& constantDefinitionString = "") const {
            std::pair<std::shared_ptr<MT>, std::vector<std::shared_ptr<storm::logic::Formula const>>> result;
            storm::prism::Program program = storm::api::parseProgram(pathToPrismFile);
            program = storm::utility::prism::preprocess(program, constantDefinitionString);
            result.second = storm::api::extractFormulasFromProperties(storm::api::parsePropertiesForPrismProgram(formulasAsString, program));
            result.first = storm::api::buildSymbolicModel<TestType::ddType, ValueType>(program, result.second)->template as<MT>();
            return result;
        }
        
        std::vector<storm::modelchecker::CheckTask<storm::logic::Formula, ValueType>> getTasks(std::vector<std::shared_ptr<storm::logic::Formula const>> const& formulas) const {
            std::vector<storm::modelchecker::CheckTask<storm::logic::Formula, ValueType>> result;
            for (auto const& f : formulas) {
                result.emplace_back(*f);
            }
            return result;
        }
        
        template <typename MT = typename TestType::ModelType>
        typename std::enable_if<std::is_same<MT, SparseModelType>::value, std::shared_ptr<storm::modelchecker::AbstractModelChecker<MT>>>::type
        createModelChecker(std::shared_ptr<MT> const& model) const {
            if (TestType::engine == storm::settings::modules::CoreSettings::Engine::Sparse) {
                return std::make_shared<storm::modelchecker::SparseDtmcPrctlModelChecker<SparseModelType>>(*model);
            }
        }
        
        template <typename MT = typename TestType::ModelType>
        typename std::enable_if<std::is_same<MT, SymbolicModelType>::value, std::shared_ptr<storm::modelchecker::AbstractModelChecker<MT>>>::type
        createModelChecker(std::shared_ptr<MT> const& model) const {
            if (TestType::engine == storm::settings::modules::CoreSettings::Engine::Hybrid) {
                return std::make_shared<storm::modelchecker::HybridDtmcPrctlModelChecker<SymbolicModelType>>(*model);
            } else if (TestType::engine == storm::settings::modules::CoreSettings::Engine::Dd) {
                return std::make_shared<storm::modelchecker::SymbolicDtmcPrctlModelChecker<SymbolicModelType>>(*model);
            }
        }
        
        bool getQualitativeResultAtInitialState(std::shared_ptr<storm::models::Model<ValueType>> const& model, std::unique_ptr<storm::modelchecker::CheckResult>& result) {
            auto filter = getInitialStateFilter(model);
            result->filter(*filter);
            return result->asQualitativeCheckResult().forallTrue();
        }
        
        ValueType getQuantitativeResultAtInitialState(std::shared_ptr<storm::models::Model<ValueType>> const& model, std::unique_ptr<storm::modelchecker::CheckResult>& result) {
            auto filter = getInitialStateFilter(model);
            result->filter(*filter);
            return result->asQuantitativeCheckResult<ValueType>().getMin();
        }
    
    private:
        storm::Environment _environment;
        
        std::unique_ptr<storm::modelchecker::QualitativeCheckResult> getInitialStateFilter(std::shared_ptr<storm::models::Model<ValueType>> const& model) const {
            if (isSparseModel()) {
                return std::make_unique<storm::modelchecker::ExplicitQualitativeCheckResult>(model->template as<SparseModelType>()->getInitialStates());
            } else {
                return std::make_unique<storm::modelchecker::SymbolicQualitativeCheckResult<TestType::ddType>>(model->template as<SymbolicModelType>()->getReachableStates(), model->template as<SymbolicModelType>()->getInitialStates());
            }
        }
    };
  
    typedef ::testing::Types<
            SparseGmmxxGmresIluEnvironment,
            SparseGmmxxGmresDiagEnvironment,
            SparseGmmxxBicgstabIluEnvironment,
            SparseEigenDGmresEnvironment,
            SparseEigenDoubleLUEnvironment,
            SparseEigenRationalLUEnvironment,
            SparseRationalEliminationEnvironment,
            SparseNativeJacobiEnvironment,
            SparseNativeWalkerChaeEnvironment,
            SparseNativeSorEnvironment,
            SparseNativePowerEnvironment,
            SparseNativeSoundPowerEnvironment,
            SparseNativeRationalSearchEnvironment,
            HybridSylvanGmmxxGmresEnvironment,
            HybridCuddNativeJacobiEnvironment,
            HybridCuddNativeSoundPowerEnvironment,
            HybridSylvanNativeRationalSearchEnvironment,
            DdSylvanNativePowerEnvironment,
            DdCuddNativeJacobiEnvironment //,
            DdSylvanRationalSearchEnvironment
        > TestingTypes;
    
    TYPED_TEST_CASE(DtmcPrctlModelCheckerTest, TestingTypes);

    TYPED_TEST(DtmcPrctlModelCheckerTest, Die) {
        std::string formulasString = "P=? [F \"one\"]";
        formulasString += "; P=? [F \"two\"]";
        formulasString += "; P=? [F \"three\"]";
        formulasString += "; R=? [F \"done\"]";
        
        auto modelFormulas = this->buildModelFormulas(STORM_TEST_RESOURCES_DIR "/dtmc/die.pm", formulasString);
        auto model = std::move(modelFormulas.first);
        auto tasks = this->getTasks(modelFormulas.second);
        EXPECT_EQ(13ul, model->getNumberOfStates());
        EXPECT_EQ(20ul, model->getNumberOfTransitions());
        ASSERT_EQ(model->getType(), storm::models::ModelType::Dtmc);
        auto checker = this->createModelChecker(model);
        std::unique_ptr<storm::modelchecker::CheckResult> result;
        
        result = checker->check(this->env(), tasks[0]);
        EXPECT_NEAR(this->parseNumber("1/6"), this->getQuantitativeResultAtInitialState(model, result), this->precision());
    
        result = checker->check(this->env(), tasks[1]);
        EXPECT_NEAR(this->parseNumber("1/6"), this->getQuantitativeResultAtInitialState(model, result), this->precision());
    
        result = checker->check(this->env(), tasks[2]);
        EXPECT_NEAR(this->parseNumber("1/6"), this->getQuantitativeResultAtInitialState(model, result), this->precision());
    
        result = checker->check(this->env(), tasks[3]);
        EXPECT_NEAR(this->parseNumber("11/3"), this->getQuantitativeResultAtInitialState(model, result), this->precision());
    }
    
    TYPED_TEST(DtmcPrctlModelCheckerTest, Crowds) {
        std::string formulasString = "P=? [F observe0>1]";
        formulasString += "; P=? [F \"observeIGreater1\"]";
        formulasString += "; P=? [F observe1>1]";
        
        auto modelFormulas = this->buildModelFormulas(STORM_TEST_RESOURCES_DIR "/dtmc/crowds-4-3.pm", formulasString);
        auto model = std::move(modelFormulas.first);
        auto tasks = this->getTasks(modelFormulas.second);
        EXPECT_EQ(726ul, model->getNumberOfStates());
        EXPECT_EQ(1146ul, model->getNumberOfTransitions());
        ASSERT_EQ(model->getType(), storm::models::ModelType::Dtmc);
        auto checker = this->createModelChecker(model);
        std::unique_ptr<storm::modelchecker::CheckResult> result;
        
        result = checker->check(this->env(), tasks[0]);
        EXPECT_NEAR(this->parseNumber("78686542099694893/1268858272000000000"), this->getQuantitativeResultAtInitialState(model, result), this->precision());
    
        result = checker->check(this->env(), tasks[1]);
        EXPECT_NEAR(this->parseNumber("40300855878315123/1268858272000000000"), this->getQuantitativeResultAtInitialState(model, result), this->precision());
    
        result = checker->check(this->env(), tasks[2]);
        EXPECT_NEAR(this->parseNumber("13433618626105041/1268858272000000000"), this->getQuantitativeResultAtInitialState(model, result), this->precision());
    
    }
    
    TYPED_TEST(DtmcPrctlModelCheckerTest, SynchronousLeader) {
        std::string formulasString = "P=? [F \"elected\"]";
        formulasString += "; P=? [F<=5 \"elected\"]";
        formulasString += "; R=? [F \"elected\"]";
        
        auto modelFormulas = this->buildModelFormulas(STORM_TEST_RESOURCES_DIR "/dtmc/leader-3-5.pm", formulasString);
        auto model = std::move(modelFormulas.first);
        auto tasks = this->getTasks(modelFormulas.second);
        EXPECT_EQ(273ul, model->getNumberOfStates());
        EXPECT_EQ(397ul, model->getNumberOfTransitions());
        ASSERT_EQ(model->getType(), storm::models::ModelType::Dtmc);
        auto checker = this->createModelChecker(model);
        std::unique_ptr<storm::modelchecker::CheckResult> result;
        
        result = checker->check(this->env(), tasks[0]);
        EXPECT_NEAR(this->parseNumber("1"), this->getQuantitativeResultAtInitialState(model, result), this->precision());
        
        result = checker->check(this->env(), tasks[1]);
        EXPECT_NEAR(this->parseNumber("24/25"), this->getQuantitativeResultAtInitialState(model, result), this->precision());
        
        result = checker->check(this->env(), tasks[2]);
        EXPECT_NEAR(this->parseNumber("25/24"), this->getQuantitativeResultAtInitialState(model, result), this->precision());
    }


}