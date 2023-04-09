function x = mj_busExist(busName)
% Copyright 2022-2023 The MathWorks, Inc.
    x=evalin('base', strcat("exist('", busName, "', 'var')"));
end