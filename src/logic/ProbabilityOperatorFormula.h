#ifndef STORM_LOGIC_PROBABILITYOPERATORFORMULA_H_
#define STORM_LOGIC_PROBABILITYOPERATORFORMULA_H_

#include "src/logic/OperatorFormula.h"

namespace storm {
    namespace logic {
        class ProbabilityOperatorFormula : public OperatorFormula {
        public:
            ProbabilityOperatorFormula(std::shared_ptr<Formula const> const& subformula);
            ProbabilityOperatorFormula(ComparisonType comparisonType, double bound, std::shared_ptr<Formula const> const& subformula);
            ProbabilityOperatorFormula(OptimizationDirection optimalityType, ComparisonType comparisonType, double bound, std::shared_ptr<Formula const> const& subformula);
            ProbabilityOperatorFormula(OptimizationDirection optimalityType, std::shared_ptr<Formula const> const& subformula);
            ProbabilityOperatorFormula(boost::optional<OptimizationDirection> optimalityType, boost::optional<ComparisonType> comparisonType, boost::optional<double> bound, std::shared_ptr<Formula const> const& subformula);

            virtual ~ProbabilityOperatorFormula() {
                // Intentionally left empty.
            }
            
            virtual bool isPctlStateFormula() const override;
            virtual bool isCslStateFormula() const override;
            virtual bool isPltlFormula() const override;
            virtual bool containsProbabilityOperator() const override;
            virtual bool containsNestedProbabilityOperators() const override;
            
            virtual bool isProbabilityOperatorFormula() const override;
            
            virtual std::ostream& writeToStream(std::ostream& out) const override;
        };
    }
}

#endif /* STORM_LOGIC_PROBABILITYOPERATORFORMULA_H_ */