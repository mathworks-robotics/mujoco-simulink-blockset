%% Package into zip

% folder name, {files}
fileList = {
    'blocks', {'*.xml', '*.m', '*.mex*', '*.pdb', '*.slx'}
    'examples', {'*.slx'}
    'src', {'*'}
    'tools', {'*.m', '*.js', '*.mk', '*.json', 'Makefile'}
    '.', {'*.m', '*.txt', '*.md'}
    };

curDir = pwd;
cd ..

% Delete the package folder
MJ_VER = getpref('mujoco', 'MJ_VER');
packageFolder = ['Release_', computer('arch'), '_', version('-release'), '_', MJ_VER];
status = rmdir(['../', packageFolder], 's');

% Copy all files and folders to package folder
copyFiles(['../', packageFolder], fileList)

cd(curDir)

function copyFiles(packageFolder, fileList)
failCount = 0;
for folderIndex=1:length(fileList(:,1))
    folder = fileList{folderIndex, 1};
    for fileIndex=1:length(fileList{folderIndex, 2})
        files = fileList{folderIndex, 2};
        file = files{fileIndex};
        status = copyfile(fullfile(folder, file), fullfile(packageFolder, folder));
        if status == 0
            failCount = failCount+1;
        end
    end
end
disp(failCount)
end