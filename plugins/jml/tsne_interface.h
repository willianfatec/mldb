/** tsne.h                                                          -*- C++ -*-
    Jeremy Barnes, 16 December 2014
    Copyright (c) 2014 mldb.ai inc.  All rights reserved.

    This file is part of MLDB. Copyright 2015 mldb.ai inc. All rights reserved.

    TSNE algorithm for a dataset.
*/

#pragma once

#include "mldb/core/dataset.h"
#include "mldb/core/procedure.h"
#include "mldb/core/value_function.h"
#include "mldb/builtin/matrix.h"
#include "mldb/types/value_description_fwd.h"


namespace MLDB {

struct TsneItl;

struct TsneConfig : public ProcedureConfig {
    static constexpr const char * name = "tsne.train";

    InputQuery trainingData;

    /// Dataset config to hold the output embedding
    PolyConfigT<Dataset> output = DefaultType("embedding");

    Url modelFileUrl;  ///< URI to save the artifact output by t-SNE training

    int numInputDimensions = -1;
    int numOutputDimensions = 2;
    double tolerance = 1e-5;
    double perplexity = 30.0;
    double learningRate = 500.0;
    int minIterations = 200;
    int maxIterations = 1000;
    
    Utf8String functionName;
};

DECLARE_STRUCTURE_DESCRIPTION(TsneConfig);



/*****************************************************************************/
/* TSNE PROCEDURE                                                             */
/*****************************************************************************/

// Input: a dataset, training parameters
// Output: a version, which has an artifact (TSNE file), a configuration, ...
// the important thing is that it can be deployed as a function, both internally
// or externally
struct TsneProcedure: public Procedure {

    TsneProcedure(MldbEngine * owner,
                  PolyConfig config,
                  const std::function<bool (const Json::Value &)> & onProgress);

    virtual RunOutput run(const ProcedureRunConfig & run,
                          const std::function<bool (const Json::Value &)> & onProgress) const;

    virtual Any getStatus() const;

    TsneConfig tsneConfig;
};

struct TsneEmbedConfig {
    TsneEmbedConfig(const Url & modelFileUrl = Url())
        : modelFileUrl(modelFileUrl)
    {
    }

    Url modelFileUrl;
};

DECLARE_STRUCTURE_DESCRIPTION(TsneEmbedConfig);


/*****************************************************************************/
/* TSNE EMBED ROW                                                            */
/*****************************************************************************/

struct TsneInput {
    ExpressionValue embedding;
};

DECLARE_STRUCTURE_DESCRIPTION(TsneInput);

struct TsneOutput {
    ExpressionValue tsne;
};

DECLARE_STRUCTURE_DESCRIPTION(TsneOutput);

struct TsneEmbed: public ValueFunctionT<TsneInput, TsneOutput> {
    TsneEmbed(MldbEngine * owner,
              PolyConfig config,
              const std::function<bool (const Json::Value &)> & onProgress);
    
    virtual TsneOutput call(TsneInput input) const override;
    
    TsneEmbedConfig functionConfig;
    std::shared_ptr<TsneItl> itl;
};

} // namespace MLDB

