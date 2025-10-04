function install()
% Copyright 2022-2025 The MathWorks, Inc.

%% Run this script once and follow the instructions
% Make sure you run this script from this folder.

MJ_VER = '3.3.6';
GLFW_VER = '3.3.7';
urlsList = fileread("tools/links.json");
blockPath = './blocks/';
examplePath = './examples/';
srcPath = './src/';

glfwRunTimeLib = 'lib-static-ucrt';
% Universal C Run Time (UCRT) is installed in windows 10 or beyond by
% default

%% DOWNLOAD MUJOCO
clear mex %#ok<CLMEX> 
mjPath = downloader(urlsList, "mujoco", MJ_VER);
if ~ispc
    folder = fullfile(mjPath, ['mujoco-', MJ_VER]);
    copyfile(fullfile(folder, '*'), mjPath);
    rmdir(folder, 's');
end

%% DOWNLOAD GLFW
if ispc
    glfwTopPath = downloader(urlsList, "glfw", GLFW_VER);
end

%% MATLAB PATH ADDITION
addpath(blockPath);
addpath(examplePath);
savepath
disp(' ')
disp("MuJoCo block library and examples added to MATLAB path and saved");

%% SHARED LIB COPY PATH ADDITION
% Alternatively you can add the dll location to system path
mujocoIncPath = fullfile(mjPath, 'include');
mujocoImportLibPath = fullfile(mjPath, 'lib');

if ispc
    mujocoDll = fullfile(mjPath, 'bin', 'mujoco.dll');
    copyfile(mujocoDll, blockPath);

    glfwPath = fullfile(glfwTopPath, ['glfw-', GLFW_VER, '.bin.WIN64']);
    glfwIncPath = fullfile(glfwPath, 'include');
    glfwImportLibPath = fullfile(glfwPath, glfwRunTimeLib);

    glfwDll = fullfile(glfwImportLibPath, 'glfw3.dll');
    copyfile(glfwDll, blockPath);
else
    mjSo = fullfile(mjPath, 'lib', ['libmujoco.so.', MJ_VER]);
    copyfile(mjSo, blockPath);
end

%% Path (MATLAB)
addpath(srcPath);
addpath(mujocoIncPath);
if ispc
    addpath(glfwIncPath);
    linkPaths = {glfwImportLibPath, mujocoImportLibPath};
    incPaths = {mujocoIncPath, fullfile(pwd, srcPath), glfwIncPath};
else
    linkPaths = {mujocoImportLibPath};
    incPaths = {mujocoIncPath, fullfile(pwd, srcPath)};
end
savepath

%% MEX Build configuration
if ispref('mujoco')
    rmpref('mujoco');
end

addpref('mujoco', 'MJ_VER', MJ_VER);
addpref('mujoco', 'linkPaths', linkPaths);
addpref('mujoco', 'incPaths', incPaths);
addpref('mujoco', 'srcPaths', {fullfile(pwd, srcPath)});
if ispc
    addpref('mujoco', 'glfwIncPath', glfwIncPath);
    addpref('mujoco', 'glfwImportLibPath', glfwImportLibPath);
end

%% Local functions
    function downloadfolder = downloader(urlsList, name, version)
    libraryFolder = 'lib';
    downloadfolder = fullfile(pwd, libraryFolder, computer('arch'), name);
    
    if strcmp(name, 'mujoco')
        % Generate MuJoCo URL based on version and architecture
        downloadLink = generateMujocoUrl(version);
    else
        % Use JSON for other libraries (GLFW)
        urlJson = jsondecode(urlsList);
        downloadLink = '';
        for i=1:length(urlJson.files)
            obj = urlJson.files(i);
            if strcmp(obj.name, name) && strcmp(obj.version, version) && strcmp(obj.arch, computer('arch'))
                downloadLink = obj.downloadLink;
                break;
            end
        end
        
        if isempty(downloadLink)
            error('Download link not found for %s version %s on %s', name, version, computer('arch'));
        end
    end
    
    disp(' ')
    disp('Download URL is:');
    disp(downloadLink);

    if isfolder(downloadfolder)
        disp(' ')
        disp('folder exists already. it will be overwritten');
    end

    [~,~,ext]=fileparts(downloadLink);
    downloadFile = fullfile(libraryFolder, strcat('download', ext));
    status = mkdir(libraryFolder); %#ok<NASGU> 
    websave(downloadFile, downloadLink);
    if strcmp(ext,'.zip')
        unzip(downloadFile, downloadfolder);
    elseif strcmp(ext, '.gz')
        untar(downloadFile, downloadfolder);
    else
        error('unknown extension. Unable to extract archive');
    end
    disp(' ')
    disp(name + ' downloaded and extracted to this path:');
    disp(downloadfolder)
end

function url = generateMujocoUrl(version)
    baseUrl = 'https://github.com/deepmind/mujoco/releases/download/';
    
    switch computer('arch')
        case 'win64'
            platform = 'windows-x86_64';
            ext = '.zip';
        case 'glnxa64'
            platform = 'linux-x86_64';
            ext = '.tar.gz';
        case 'maci64'
            platform = 'macos-universal2';
            ext = '.dmg';
        case 'maca64'
            % For Apple Silicon Macs
            platform = 'macos-universal2';
            ext = '.dmg';
        otherwise
            error('Unsupported architecture: %s', computer('arch'));
    end
    
    % Construct the URL: version/mujoco-version-platform.ext
    url = sprintf('%s%s/mujoco-%s-%s%s', baseUrl, version, version, platform, ext);
end
end
