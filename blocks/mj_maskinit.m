% Copyright 2022-2023 The MathWorks, Inc.
mjBlk = gcb;
mo = get_param(mjBlk,'MaskObject');
%% Sensor Bus Config
blankBusPath = [mjBlk, '/sensorToBus/blankSensorBus'];
sensorToBusPath = [mjBlk, '/sensorToBus'];
sensorOut1Path = [mjBlk, '/sensor'];
portIndex = 1;
try
    sensorFieldnames = fieldnames(Simulink.Bus.createMATLABStruct(sensorBus));
    %fails when it is an empty bus TODO. remove try catch
catch
    sensorFieldnames = [];
end

if isempty(sensorFieldnames)
    mo.getDialogControl('sensorBusText').Prompt = ['Sensor Bus Type: ', 'NA'];
    set_param(sensorToBusPath, 'Commented', 'through');
    replacer(mjBlk, 'sensor', 'simulink/Sinks/Terminator')
else
    mo.getDialogControl('sensorBusText').Prompt = ['Sensor Bus Type: ', sensorBus];
    set_param(sensorToBusPath, 'Commented', 'off');
    outportName = 'sensor';
    replacer(mjBlk, outportName, 'simulink/Sinks/Out1');
    set_param([mjBlk, '/', outportName], "Port", num2str(portIndex));
    portIndex = portIndex+1;

    set_param(blankBusPath, 'OutDataTypeStr', ['Bus: ', sensorBus]);
    set_param([sensorToBusPath '/parser'], 'OutputBusName', sensorBus);
end

%% Control Bus Config
controlInportPath = [mjBlk, '/', 'u'];
uToVectorPath = [mjBlk, '/', 'uToVector'];
uPortExpander = [mjBlk, '/uPortExpander'];
try
    controlFieldnames = fieldnames(Simulink.Bus.createMATLABStruct(controlBus));
    %fails when controlBus is an empty bus TODO. remove try catch
catch
    controlFieldnames = [];
end

% Remove inport for passive simulation case
if isempty(controlFieldnames)
    replacer(mjBlk, 'u', 'simulink/Sources/Ground');
    set_param(uToVectorPath, 'Commented', 'through');
    mo.getDialogControl('controlBusText').Prompt = 'Control Vector Type: NA';
    set_param(uPortExpander, 'Commented', 'through');
else
    set_param(uPortExpander, 'Commented', 'off');
    replacer(mjBlk, 'u', 'simulink/Sources/In1');
    % Set bus or vector
    if strcmp(get_param(mjBlk, 'controlInterfaceType'), 'Bus')
        set_param(controlInportPath, 'OutDataTypeStr', ['Bus: ', controlBus]);
        set_param(uToVectorPath, 'Commented', 'off');
        mo.getDialogControl('controlBusText').Prompt = ['Control Bus Type: ', controlBus];
        set_param(controlInportPath, 'PortDimensions', '-1')
    elseif strcmp(get_param(mjBlk, 'controlInterfaceType'), 'Vector')
        set_param(controlInportPath, 'OutDataTypeStr', 'Inherit: auto');
        set_param(uToVectorPath, 'Commented', 'through');
        mo.getDialogControl('controlBusText').Prompt = ['Control Vector Type: [', strjoin(controlFieldnames, ', '), ']'];
        inputDim = [length(controlFieldnames) 1];
        set_param(controlInportPath, 'PortDimensions', ['[' num2str(inputDim), ']']);
    end
end

%% RGB

% blankBusPath = [mjBlk, '/rgbToBus/blankBus'];
% rgbToBusPath = [mjBlk, '/rgbToBus'];
rgbOut1Path = [mjBlk, '/rgb'];
try
    rgbFieldnames = fieldnames(Simulink.Bus.createMATLABStruct(rgbBus));
    %fails when it is an empty bus TODO. remove try catch
    mo.getDialogControl('rgbBusText').Prompt = ['RGB Bus Type: ', rgbBus];
catch
    rgbFieldnames = [];
    mo.getDialogControl('rgbBusText').Prompt = ['RGB Bus Type: ', 'NA'];
end
 
if isempty(rgbFieldnames)
    replacer(mjBlk, 'rgb', 'simulink/Sinks/Terminator')
else
    outportName = 'rgb';
    replacer(mjBlk, outportName, 'simulink/Sinks/Out1');
    set_param([mjBlk, '/', outportName], "Port", num2str(portIndex));
    portIndex = portIndex+1;
end

%% Depth
mo.getDialogControl('rgbBusText').Prompt = ['RGB Bus Type: ', rgbBus];
depthOut1Path = [mjBlk, '/depth'];
depthConverterPath = [mjBlk, '/OpenGL Depth conversion'];
try
    depthFieldnames = fieldnames(Simulink.Bus.createMATLABStruct(depthBus));
    %fails when it is an empty bus TODO. remove try catch
    mo.getDialogControl('depthBusText').Prompt = ['Depth Bus Type: ', depthBus];
catch
    depthFieldnames = [];
    mo.getDialogControl('depthBusText').Prompt = ['Depth Bus Type: ', 'NA'];
end
depthOutputOption = strcmp(get_param(mjBlk, 'depthOutOption'), 'on');

if isempty(depthFieldnames) || ~depthOutputOption
    set_param(depthConverterPath, 'Commented', 'on');
    replacer(mjBlk, 'depth', 'simulink/Sinks/Terminator')
else
    set_param(depthConverterPath, 'Commented', 'off');
    outportName = 'depth';
    replacer(mjBlk, 'depth', 'simulink/Sinks/Out1');
    set_param([mjBlk, '/', outportName], "Port", num2str(portIndex));
    portIndex = portIndex+1;
end

[znear, zfar] = mj_depth_near_far(xmlFile);
set_param(mjBlk, 'znear', num2str(znear));
set_param(mjBlk, 'zfar', num2str(zfar));

%% Sample Time
sampleTime = mj_sampletime(xmlFile);
set_param(mjBlk, 'sampleTime', num2str(sampleTime));

mo.getDialogControl('sampleTimeText').Prompt = ['Sample Time: ', num2str(sampleTime)];

function replacer(blk, oldname, newtype)
    oldpath = [blk, '/', oldname];
    newTypeWithoutLib = strsplit(newtype, '/');
    newTypeWithoutLib = newTypeWithoutLib{end};
    if strcmp(newTypeWithoutLib, 'In1')
        newTypeWithoutLib = 'Inport';
    elseif strcmp(newTypeWithoutLib, 'Out1')
        newTypeWithoutLib = 'Outport';
    end
    if ~strcmp(get_param(oldpath, 'BlockType'), newTypeWithoutLib)
        position =  get_param(oldpath, 'Position');
        delete_block(oldpath);
        add_block(newtype, oldpath, 'Position', position);
    end
end