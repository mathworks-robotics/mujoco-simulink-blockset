// This mex function generates a Simulink bus using "Simulink.Bus.createObject"
//  1. The generated bus is named uniquely using std::hash
//  2. If a bus with same name already exists, it will not regenerate
//  3. std::hash is assumed to return unique hash within a MATLAB instance

// MATLAB and Simulink are registered trademarks of The MathWorks, Inc.
// Copyright 2022-2023 The MathWorks, Inc.

#include "mex.hpp"
#include "mexAdapter.hpp"
#include "MatlabDataArray.hpp"
#include "mj.hpp"
#include <iostream>
#include <mutex>

static std::mutex mut;

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
        // serialize this function as it accesses files and base workspace
        // Donot want one call of this function to delete a temporary base workspace variable while another instance is still using it
        
        std::lock_guard<std::mutex> lock(mut);

        using namespace matlab::data;
        using namespace matlab::mex;
        using namespace matlab::engine;

        if(inputs.size() != 1)
        {
            printError("Expected 1 input");
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

        std::shared_ptr<MujocoModelInstance> mi = std::make_shared<MujocoModelInstance>();
        if(mi->initMdl(pathStr) != 0)
        {
            printError("Unable to load file");
        }

        int outputIndex = 0;

        // input bus
        auto ci = mi->ci;
        std::string controlBusStr = inputBusGen(ci);
        outputs[outputIndex++] = af.createCharArray(controlBusStr);

        // sensor bus
        auto si = mi->si;
        std::string sensorBusStr = sensorBusGen(si);
        outputs[outputIndex++] = af.createCharArray(sensorBusStr);

        // rgb bus
        auto cami = mi->cami;
        std::string rgbBusStr = rgbBusGen(cami);
        outputs[outputIndex++] = af.createCharArray(rgbBusStr);

        // depth bus
        std::string depthBusStr = depthBusGen(cami);
        outputs[outputIndex++] = af.createCharArray(depthBusStr);

        // data length output
        ArrayDimensions lengthOutputdim{4, 1};
        outputs[outputIndex++] = af.createArray<uint32_t>(lengthOutputdim, {ci.count, si.scalarCount, cami.rgbLength, cami.depthLength});

        // displayOnMATLAB(stream);   

        glfwTerminate();
    }

    std::string sensorBusGen(sensorInterface si)
    {
        using namespace matlab::data;
        using namespace matlab::mex;
        using namespace matlab::engine;

        // struct generation
        StructArray busStruct = af.createStructArray({1}, si.names);
        for(unsigned int index=0; index<si.count; index++)
        {
            std::string name = si.names[index];
            ArrayDimensions sensorDim{si.dim[index],1};
            busStruct[0][name] = af.createArray<double>(sensorDim);
        }

        // name the bus with a identifier unique to block outputs/inputs
        std::string busStr="mj_bus_sensor_";
        busStr += std::to_string(si.hash());

        return busGen(busStr, busStruct);
    }

    std::string inputBusGen(controlInterface ci)
    {
        using namespace matlab::data;
        using namespace matlab::mex;
        using namespace matlab::engine;

        // struct generation
        StructArray busStruct = af.createStructArray({1}, ci.names);
        for(unsigned int index=0; index<ci.count; index++)
        {
            std::string name = ci.names[index];
            busStruct[0][name] = af.createArray<double>({1,1});
        }

        // name the bus with a identifier unique to block outputs/inputs
        std::string busStr="mj_bus_input_";
        busStr += std::to_string(ci.hash());

        return busGen(busStr, busStruct);
    }

    std::string rgbBusGen(cameraInterface cami)
    {
        using namespace matlab::data;
        using namespace matlab::mex;
        using namespace matlab::engine;

        // struct generation
        StructArray busStruct = af.createStructArray({1}, cami.names);
        for(unsigned int index=0; index<cami.count; index++)
        {
            std::string name = cami.names[index];
            ArrayDimensions outputDim{cami.size[index].height, cami.size[index].width, 3};
            busStruct[0][name] = af.createArray<uint8_t>(outputDim);
        }

        // name the bus with a identifier unique to block outputs/inputs
        std::string busStr="mj_bus_rgb_";
        busStr += std::to_string(cami.hash());

        return busGen(busStr, busStruct);
    }

    std::string depthBusGen(cameraInterface cami)
    {
        using namespace matlab::data;
        using namespace matlab::mex;
        using namespace matlab::engine;

        // struct generation
        StructArray busStruct = af.createStructArray({1}, cami.names);
        for(unsigned int index=0; index<cami.count; index++)
        {
            std::string name = cami.names[index];
            ArrayDimensions outputDim{cami.size[index].height, cami.size[index].width};
            busStruct[0][name] = af.createArray<float>(outputDim);
        }

        // name the bus with a identifier unique to block outputs/inputs
        std::string busStr="mj_bus_depth_";
        busStr += std::to_string(cami.hash());

        return busGen(busStr, busStruct);
    }

    std::string busGen(std::string busStr, matlab::data::StructArray busStruct)
    {
        using namespace matlab::data;
        using namespace matlab::mex;
        using namespace matlab::engine;

        // bus generation from struct
        auto outputStream = std::shared_ptr<matlab::engine::StreamBuffer>();
        auto errorStream = std::shared_ptr<matlab::engine::StreamBuffer>();

        // check for existence of bus
        std::vector<matlab::data::Array> args({af.createScalar(busStr)});

        std::vector<matlab::data::Array> result;
        result = matlabPtr->feval(u"mj_busExist", 1, args, outputStream, errorStream);

        matlab::data::TypedArray<double> returnedValues(std::move(result[0]));
        double alreadyExistsDouble = returnedValues[0];

        int alreadyExists = static_cast<int>(std::round(alreadyExistsDouble));

        if (!alreadyExists)
        {
            // If exists, do not generate the bus again
            auto arg = std::vector<matlab::data::Array>({busStruct});
            auto outputVector = matlabPtr->feval(u"Simulink.Bus.createObject", 1, arg, outputStream, errorStream);

            matlab::data::StructArray outputStructArray = outputVector[0];

            auto val = outputStructArray[0]["busName"];
            if(val.getType() == ArrayType::CHAR)
            {
                CharArray valChar = val;
                std::string clearCmd = "clear('" + valChar.toAscii() + "');";
                auto busTempVariable = matlabPtr->getVariable(valChar.toAscii(), WorkspaceType::BASE);
                matlabPtr->setVariable(busStr, busTempVariable, WorkspaceType::BASE);
                matlabPtr->eval(convertUTF8StringToUTF16String("evalin base "+clearCmd));
            }
            else
            {
                printError("busName field has to be a char array");
            }
        }
        return busStr;
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

