#ifndef STORM_ADAPTERS_CARLADAPTER_H_
#define STORM_ADAPTERS_CARLADAPTER_H_

// Include config to know whether CARL is available or not.
#include "storm-config.h"

#ifdef STORM_HAVE_CARL

#include <cln/cln.h>
#include <gmpxx.h>
#include <carl/core/MultivariatePolynomial.h>
#include <carl/core/RationalFunction.h>
#include <carl/core/VariablePool.h>
#include <carl/core/FactorizedPolynomial.h>
#include <carl/core/Relation.h>
#include <carl/core/SimpleConstraint.h>

namespace carl {
    // Define hash values for all polynomials and rational function.
    template<typename C, typename O, typename P>
    inline size_t hash_value(carl::MultivariatePolynomial<C,O,P> const& p) {
        std::hash<carl::MultivariatePolynomial<C,O,P>> h;
        return h(p);
    }
    
    template<typename Pol>
    inline size_t hash_value(carl::FactorizedPolynomial<Pol> const& p) {
        std::hash<FactorizedPolynomial<Pol>> h;
        return h(p);
    }

    template<typename Pol>
    inline size_t hash_value(carl::RationalFunction<Pol> const& f)  {
        std::hash<Pol> h;
        return h(f.nominator()) ^ h(f.denominator());
    }
    
    template<typename Number>
    inline size_t hash_value(carl::Interval<Number> const& i) {
        std::hash<Interval<Number>> h;
        return h(i);
    }
}

namespace storm {
    typedef mpq_class RationalNumber;
    typedef carl::Variable Variable;
    typedef carl::MultivariatePolynomial<RationalNumber> RawPolynomial;
    typedef carl::FactorizedPolynomial<RawPolynomial> Polynomial;
    typedef carl::Relation CompareRelation;
    
    typedef carl::RationalFunction<Polynomial> RationalFunction;
    typedef carl::Interval<double> Interval;
    template<typename T> using ArithConstraint = carl::SimpleConstraint<T>;
}

#endif

#endif /* STORM_ADAPTERS_CARLADAPTER_H_ */