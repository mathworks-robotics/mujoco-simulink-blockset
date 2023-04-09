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
            printError("Unable to load and init file");
        }

        // https://github.com/deepmind/dm_control/blob/20cef21e2554592cd7fad0bb32c169aff2fe72bc/dm_control/mujoco/engine.py#L861
        // Based on,
        //  http://stackoverflow.com/a/6657284/1461210
        //  https://www.khronos.org/opengl/wiki/Depth_Buffer_Precision
        double extent = mi.get_m()->stat.extent;
        double znear = mi.get_m()->vis.map.znear*extent;
        double zfar = mi.get_m()->vis.map.zfar*extent;

        outputs[0] = af.createScalar(znear);
        outputs[1] = af.createScalar(zfar);

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

