function mj_postcodegen_grt(buildInfo)
% Copyright 2022-2023 The MathWorks, Inc.
    buildInfo.addCompileFlags('-O3 -fPIC','OPTS');

    buildInfo.addIncludePaths(getpref('mujoco', 'incPaths'));
    buildInfo.addSourcePaths(getpref('mujoco', 'srcPaths'));

    buildInfo.addSourceFiles({'mj_sfun.cpp' 'mj.cpp'});

    linkPaths = getpref('mujoco', 'linkPaths');
    linkObjs = {'glfw3dll.lib', 'mujoco.lib'};
    libPriority = '';
    libPreCompiled = true;
    libLinkOnly = true;
    for i = 1:length(linkObjs)
        buildInfo.addLinkObjects(linkObjs{i}, linkPaths{i}, libPriority, libPreCompiled, libLinkOnly);
    end
end