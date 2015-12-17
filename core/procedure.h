// This file is part of MLDB. Copyright 2015 Datacratic. All rights reserved.

/** procedure.h                                                       -*- C++ -*-
    Jeremy Barnes, 4 December 2014
    Copyright (c) 2014 Datacratic Inc.  All rights reserved.

    Interface for procedures into MLDB.
*/

#include "mldb/types/value_description_fwd.h"
#include "mldb/core/mldb_entity.h"
#include "mldb/rest/rest_entity.h"
#include "mldb/sql/sql_expression.h"
#include "mldb/sql/sql_expression_operations.h"
#include <set>
#include <iostream>
#include <typeinfo>

// NOTE TO MLDB DEVELOPERS: This is an API header file.  No includes
// should be added, especially value_description.h.


#pragma once

namespace Datacratic {

struct RestDirectory;

namespace MLDB {

struct MldbServer;
struct Procedure;

typedef EntityType<Procedure> ProcedureType;


/*****************************************************************************/
/* PROCEDURE TRAINING                                                        */
/*****************************************************************************/

struct ProcedureRunConfig {
    Utf8String id;
    Any params;
};

DECLARE_STRUCTURE_DESCRIPTION(ProcedureRunConfig);

struct ProcedureRunStatus: public PolyStatus {
    Date runStarted;   ///< Timestamp at which run of the procedure started
    Date runFinished;  ///< Timestamp at which run of the procedure finished
};

DECLARE_STRUCTURE_DESCRIPTION(ProcedureRunStatus);

struct ProcedureRun {
    ProcedureRun()
    {
    }

    ProcedureRun(Procedure * owner,
                 ProcedureRunConfig config,
                 const std::function<bool (const Json::Value & progress)> & onProgress);

    std::shared_ptr<ProcedureRunConfig> config;
    Date runStarted;
    Date runFinished;
    Any results;
    Any details;
};

DECLARE_STRUCTURE_DESCRIPTION(ProcedureRun);

struct ProcedureRunCollection;

struct RunOutput {
    RunOutput(Any results = Any(), Any details = Any()) noexcept
        : results(std::move(results)), details(std::move(details))
    {
    }

    Any results;
    Any details;
};

DECLARE_STRUCTURE_DESCRIPTION(RunOutput);


/*****************************************************************************/
/* PROCEDURE                                                                 */
/*****************************************************************************/

/** Abstraction of a procedure.  Note that since it has child entities (its
    runs), it is necessarily a RestEntity and has to implement that
    interface.
*/

struct Procedure: public MldbEntity, public RestEntity {
    Procedure(MldbServer * server);

    virtual ~Procedure();

    MldbServer * server;

    virtual Any getStatus() const = 0;

    virtual std::string getKind() const
    {
        return "procedure";
    }

    virtual RunOutput run(const ProcedureRunConfig & run,
                          const std::function<bool (const Json::Value &)> & onProgress)
        const = 0;
    
    virtual bool isCollection() const;

    virtual Utf8String getDescription() const;

    virtual Utf8String getName() const;

    virtual RestEntity * getParent() const;

    /** Return details about a run.  Default gets the details from the
        Run object and returns directly.
    */
    virtual Any getRunDetails(const ProcedureRun * run) const;

    /**
        Apply all existing keys from the run config over the procedure config
    */
    template<typename ProcConfigType>
    ProcConfigType
    applyRunConfOverProcConf(const ProcConfigType & procConf,
                             const ProcedureRunConfig & run) const
    {
        // Recursively copy the values of b into a. Both a and b must be objects.
        typedef std::function<void (Json::Value&, const Json::Value&)> UpdateFunc;
        UpdateFunc update = [&update] (Json::Value & a, const Json::Value & b) {
            if (!a.isObject() || !b.isObject()) return;

            for (const auto & key : b.getMemberNames()) {
                if (b[key].isObject()) {
                    if(!a.isMember(key)) {
                        a[key] = Json::Value();
                    }
                    update(a[key], b[key]);
                } else {
                    a[key] = b[key];
                }
            }
        };

        Json::Value modifiedRun(jsonEncode(procConf));
        update(modifiedRun, jsonEncode(run.params));

        return jsonDecode<ProcConfigType>(modifiedRun);
    }

    std::shared_ptr<ProcedureRunCollection> runs;

};



/*****************************************************************************/
/* PROCEDURE CONFIG                                                          */
/*****************************************************************************/

/* 
 * Keep all the shared config parameters for procedure here.
 */
struct ProcedureConfig
{
    ProcedureConfig();
    bool runOnCreation; // force a run of the procedure upon creation
};

DECLARE_STRUCTURE_DESCRIPTION(ProcedureConfig);

/*****************************************************************************/
/* NULL PROCEDURE                                                            */
/*****************************************************************************/

struct NullProcedureConfig : public ProcedureConfig
{

};

DECLARE_STRUCTURE_DESCRIPTION(NullProcedureConfig);

/** Null procedure, that does nothing when run. */

struct NullProcedure: public Procedure {
    NullProcedure(MldbServer * server, const PolyConfig & config,
                 const std::function<bool (const Json::Value &)> & onProgress);

    virtual ~NullProcedure();

    virtual Any getStatus() const;

    virtual RunOutput run(const ProcedureRunConfig & run,
                          const std::function<bool (const Json::Value &)> & onProgress)
        const;
};


/*****************************************************************************/
/* SERIAL PROCEDURE                                                          */
/*****************************************************************************/

/** A serial procedure, that runs multiple procedure steps one after the
    other.
*/

struct ProcedureStepConfig: public PolyConfig {
    /// Name of the step
    Utf8String name;
};

DECLARE_STRUCTURE_DESCRIPTION(ProcedureStepConfig);

struct SerialProcedureConfig : public ProcedureConfig {
    std::vector<ProcedureStepConfig> steps;
};

DECLARE_STRUCTURE_DESCRIPTION(SerialProcedureConfig);

struct SerialProcedureStatus {
    std::vector<Any> steps;
};

DECLARE_STRUCTURE_DESCRIPTION(SerialProcedureStatus);

struct SerialProcedure: public Procedure {
    SerialProcedure(MldbServer * server,
                   const PolyConfig & config,
                   const std::function<bool (const Json::Value &)> & onProgress);

    SerialProcedureConfig config;

    virtual ~SerialProcedure();

    virtual Any getStatus() const;

    virtual RunOutput run(const ProcedureRunConfig & run,
                          const std::function<bool (const Json::Value &)> & onProgress)
        const;

    std::vector<std::shared_ptr<Procedure> > steps;
};


/*****************************************************************************/
/* CREATE ENTITY PROCEDURE                                                   */
/*****************************************************************************/

/** A procedure that creates an entity as its operation.
*/

    struct CreateEntityProcedureConfig: public PolyConfig, ProcedureConfig {
    std::string kind;  ///< function, procedure, plugin, dataset, ...
};

DECLARE_STRUCTURE_DESCRIPTION(CreateEntityProcedureConfig);

/** Output of the createEntity procedure. */
struct CreateEntityProcedureOutput {
    std::string kind;
    PolyConfig config;
    Any status;
};

DECLARE_STRUCTURE_DESCRIPTION(CreateEntityProcedureOutput);

struct CreateEntityProcedure: public Procedure {
    CreateEntityProcedure
        (MldbServer * server,
         const PolyConfig & config,
         const std::function<bool (const Json::Value &)> & onProgress);

    CreateEntityProcedureConfig config;

    virtual ~CreateEntityProcedure();

    virtual Any getStatus() const;

    virtual RunOutput run(const ProcedureRunConfig & run,
                          const std::function<bool (const Json::Value &)> & onProgress)
        const;
};


/*****************************************************************************/
/* UTILITIES                                                                 */
/*****************************************************************************/

std::shared_ptr<Procedure>
obtainProcedure(MldbServer * server,
                const PolyConfig & config,
                const std::function<bool (const Json::Value & progress)> & onProgress
                    = nullptr);


DECLARE_STRUCTURE_DESCRIPTION_NAMED(ProcedurePolyConfigDescription, PolyConfigT<Procedure>);


std::shared_ptr<ProcedureType>
registerProcedureType(const Package & package,
                      const Utf8String & name,
                      const Utf8String & description,
                      std::function<Procedure * (RestDirectory *,
                                                 PolyConfig,
                                                 const std::function<bool (const Json::Value)> &)>
                      createEntity,
                      TypeCustomRouteHandler docRoute,
                      TypeCustomRouteHandler customRoute,
                      std::shared_ptr<const ValueDescription> config,
                      std::set<std::string> registryFlags);

/** Register a new procedure kind.  This takes care of registering everything behind
    the scenes.
*/
template<typename ProcedureT, typename Config>
std::shared_ptr<ProcedureType>
registerProcedureType(const Package & package,
                      const Utf8String & name,
                      const Utf8String & description,
                      const Utf8String & docRoute,
                      TypeCustomRouteHandler customRoute = nullptr,
                      std::set<std::string> registryFlags = {})
{
    return registerProcedureType
        (package, name, description,
         [] (RestDirectory * server,
             PolyConfig config,
             const std::function<bool (const Json::Value)> & onProgress)
         {
             return new ProcedureT(ProcedureT::getOwner(server), config, onProgress);
         },
         makeInternalDocRedirect(package, docRoute),
         customRoute,
         getDefaultDescriptionSharedT<Config>(),
         registryFlags);
}

template<typename ProcedureT, typename Config>
struct RegisterProcedureType {
    RegisterProcedureType(const Package & package,
                          const Utf8String & name,
                          const Utf8String & description,
                          const Utf8String & docRoute,
                          TypeCustomRouteHandler customRoute = nullptr,
                          std::set<std::string> registryFlags = {})
    {
        handle = registerProcedureType<ProcedureT, Config>
            (package, name, description, docRoute, customRoute,
             registryFlags);
    }

    std::shared_ptr<ProcedureType> handle;
};

// VALIDATION HELPERS
template<typename ConfigType, 
    typename FieldType,
    template<typename> class Validator1>
std::function<void (ConfigType *, JsonParsingContext &)>
validate(FieldType ConfigType::* field, const char * name)
{
    return [=](ConfigType * cfg, JsonParsingContext & context)
        {
            Validator1<FieldType>()(cfg->*field, name);
        };
}

// consider using a variadic parameter
template<typename ConfigType,
    typename FieldType,
    template<typename> class Validator1,
    template<typename> class Validator2>
std::function<void (ConfigType *, JsonParsingContext & context)>
validate(FieldType ConfigType::* field, const char * name)
{
     return [=](ConfigType * cfg, JsonParsingContext & context)
        {
            Validator1<FieldType>()(cfg->*field, name);
            Validator2<FieldType>()(cfg->*field, name);
        };
}

// consider using a variadic parameter
template<typename ConfigType,
    typename FieldType,
    template<typename> class Validator1,
    template<typename> class Validator2,
    template<typename> class Validator3>
std::function<void (ConfigType *, JsonParsingContext & context)>
validate(FieldType ConfigType::* field, const char * name)
{
     return [=](ConfigType * cfg, JsonParsingContext & context)
        {
            Validator1<FieldType>()(cfg->*field, name);
            Validator2<FieldType>()(cfg->*field, name);
            Validator3<FieldType>()(cfg->*field, name);
        };
}

/** 
 *  Accept any select statement with empty GROUP BY/HAVING clause.
 *  FieldType must contain a SelectStatement named stm.
 */
template<typename FieldType> struct NoGroupByHaving 
{
    void operator()(const FieldType & query, const char * name)
    {
        if (query.stm) {
            if (!query.stm->groupBy.empty()) {
                throw ML::Exception("cannot train %s with a groupBy clause", name);
            }
            else if (!query.stm->having->isConstantTrue()) {
                throw ML::Exception("cannot train %s with a having clause", name);
            }
        }
    }
};

/**
 *  Accept simple select expressions like column1, column2, wildcard expressions
 *  and column expressions but reject operations on columns like sum(column1, column2).
 *  FieldType must contain a SelectStatement named stm.
 */
template<typename FieldType> struct PlainColumnSelect
{
    void operator()(const FieldType & query, const char * name)
    {
        auto getWildcard = [] (const std::shared_ptr<SqlRowExpression> expression) 
            -> std::shared_ptr<const WildcardExpression>
            {
                return std::dynamic_pointer_cast<const WildcardExpression>(expression);
            };

        auto getColumnExpression = [] (const std::shared_ptr<SqlRowExpression> expression)
            -> std::shared_ptr<const SelectColumnExpression>
            {
                return std::dynamic_pointer_cast<const SelectColumnExpression>(expression);
            };

        auto getComputedVariable = [] (const std::shared_ptr<SqlRowExpression> expression)
            -> std::shared_ptr<const ComputedVariable>
            {
                return std::dynamic_pointer_cast<const ComputedVariable>(expression);
            };

        auto getReadVariable = [] (const std::shared_ptr<SqlExpression> expression) 
            -> std::shared_ptr<const ReadVariableExpression>
            {
                return std::dynamic_pointer_cast<const ReadVariableExpression>(expression);
            };

        auto getWithinExpression = [] (const std::shared_ptr<SqlExpression> expression) 
            -> std::shared_ptr<const SelectWithinExpression>
            {
                return std::dynamic_pointer_cast<const SelectWithinExpression>(expression);
            };

        auto getIsTypeExpression = [] (const std::shared_ptr<SqlExpression> expression) 
            -> std::shared_ptr<const IsTypeExpression>
            {
                return std::dynamic_pointer_cast<const IsTypeExpression>(expression);
            };

        auto getComparisonExpression = [] (const std::shared_ptr<SqlExpression> expression) 
            -> std::shared_ptr<const ComparisonExpression>
            {
                return std::dynamic_pointer_cast<const ComparisonExpression>(expression);
            };

        auto getBooleanExpression = [] (const std::shared_ptr<SqlExpression> expression) 
            -> std::shared_ptr<const BooleanOperatorExpression>
            {
                return std::dynamic_pointer_cast<const BooleanOperatorExpression>(expression);
            };

        if (query.stm) {
            auto & select = query.stm->select;
            for (const auto & clause : select.clauses) {
                //std::cerr << clause->surface << std::endl;
                auto wildcard = getWildcard(clause);
                if (wildcard)
                    continue;

                auto columnExpression = getColumnExpression(clause);
                if (columnExpression)
                    continue;

                auto computedVariable = getComputedVariable(clause);
                if (computedVariable) {
                    auto readVariable = getReadVariable(computedVariable->expression);
                    if (readVariable)
                        continue;
                    // {x, y}
                    auto withinExpression = getWithinExpression(computedVariable->expression);
                    if (withinExpression)
                        continue;
                    // x is not null
                    auto isTypeExpression = getIsTypeExpression(computedVariable->expression);
                    if (isTypeExpression)
                        continue;
                    // x = 'true'
                    auto comparisonExpression = getComparisonExpression(computedVariable->expression);
                    if (comparisonExpression)
                        continue;
                    // NOT x 
                    auto booleanExpression = getBooleanExpression(computedVariable->expression);
                    if (booleanExpression)
                        continue;
                }

                throw ML::Exception(std::string(name) + 
                                    " training only accept wildcard and column names at " + 
                                    clause->surface.rawString());
            }
        }
    }
};

/**
 *  Ensure the select contains a row named "features" and a scalar named "label".
 *  FieldType must contain a SelectStatement named stm.
 */
template<typename FieldType> struct FeaturesLabelSelect
{
    void operator()(const FieldType & query, const char * name)
    {
        auto getComputedVariable = [] (const std::shared_ptr<SqlRowExpression> expression)
            -> std::shared_ptr<const ComputedVariable>
            {
                return std::dynamic_pointer_cast<const ComputedVariable>(expression);
            };

        if (query.stm) {
            auto & select = query.stm->select;
            bool foundFeatures = false;
            bool foundLabel = false;
            for (const auto & clause : select.clauses) {
                //std::cerr << clause->surface << std::endl;
                auto computedVariable = getComputedVariable(clause);
                if (computedVariable) {
                    if (computedVariable->alias == "features")
                        foundFeatures = true;
                    if (computedVariable->alias == "label")
                        foundLabel = true;
                    
                }
            }
            if (!foundFeatures || !foundLabel)
                throw ML::Exception("%s training expect a row named 'features' and a scalar named 'label'", name);
        }
    }
};

} // namespace MLDB
} // namespace Datacratic
