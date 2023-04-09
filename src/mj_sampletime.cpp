// Copyright 2022-2023 The MathWorks, Inc.
#include "mex.hpp"
#include "mexAdapter.hpp"
#include "MatlabDataArray.hpp"
#include "mj.hpp"

class MexFunction: public matlab::mex::Function
{
    private:
    std::shared_ptr<matlab::engine::MATLABEngine> matlabPtr = getEngine();
    std::ostringstream stream;
    matlab::data::ArrayFactory af;

    public:
    void operator()
    (matlab::mex::ArgumentList outputs, matlab::mex::ArgumentList inputs) 
    {

        using namespace matlab::data;
        using namespace matlab::mex;
        using namespace matlab::engine;

        if(inputs.size() != 1)
        {
            printError("1 input expected");
        }

        std::string pathStr;
        if(inputs[0].getType() == ArrayType::CHAR)
        {
            CharArray path = inputs[0];
            pathStr = path.toAscii();
        }
        else
        {
            printError("Only char array allowed as input");
        }

        MujocoModelInstance mi;
        if(mi.initMdl(pathStr, false) != 0)
        {
            printError("Unable to load file");
        }

        // sensor bus
        auto sampleTime = mi.getSampleTime();
        outputs[0] = af.createScalar(sampleTime);

        // displayOnMATLAB(stream);   
    }

    void displayOnMATLAB(std::ostringstream& stream) 
    {
        // Pass stream content to MATLAB fprintf function
        matlabPtr->feval(u"fprintf", 0, std::vector<matlab::data::Array>({ af.createScalar(stream.str()) }));
        // Clear stream buffer
        stream.str("");
    }

    void printError(std::string err)
    {
        matlabPtr->feval(u"error", 0,
                std::vector<matlab::data::Array>(
                    { af.createScalar(err) }));
    }

};

