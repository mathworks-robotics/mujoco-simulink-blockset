% Copyright 2022-2023 The MathWorks, Inc.

% Run mex -setup c++ and select a C++ compiler before running this file

clear mex %#ok<CLMEX> 
if ispc
    !gmake build
else
    !make build
end
