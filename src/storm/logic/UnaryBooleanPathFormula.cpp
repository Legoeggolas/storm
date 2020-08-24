#include "storm/logic/UnaryBooleanPathFormula.h"

#include "storm/logic/FormulaVisitor.h"

#include "storm/utility/macros.h"
#include "storm/exceptions/InvalidPropertyException.h"

namespace storm {
    namespace logic {
        UnaryBooleanPathFormula::UnaryBooleanPathFormula(OperatorType operatorType, std::shared_ptr<Formula const> const& subformula) : UnaryPathFormula(subformula), operatorType(operatorType) {
            STORM_LOG_THROW(this->getSubformula().isStateFormula() || this->getSubformula().isPathFormula(), storm::exceptions::InvalidPropertyException, "Boolean path formula must have either path or state subformulas");
        }
        
        bool UnaryBooleanPathFormula::isUnaryBooleanPathFormula() const {
            return true;
        }

        boost::any UnaryBooleanPathFormula::accept(FormulaVisitor const& visitor, boost::any const& data) const {
            return visitor.visit(*this, data);
        }
        
        UnaryBooleanPathFormula::OperatorType UnaryBooleanPathFormula::getOperator() const {
            return operatorType;
        }
        
        bool UnaryBooleanPathFormula::isNot() const {
            return this->getOperator() == OperatorType::Not;
        }
                
        std::ostream& UnaryBooleanPathFormula::writeToStream(std::ostream& out) const {
            switch (operatorType) {
                case OperatorType::Not: out << "!("; break;
            }
            this->getSubformula().writeToStream(out);
            out << ")";
            return out;
        }
    }
}
