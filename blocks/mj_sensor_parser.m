classdef (StrictDefaults)mj_sensor_parser < matlab.System
    % Copyright 2022-2023 The MathWorks, Inc.
    properties(Nontunable)
        OutputBusName = 'bus_name';
    end

    methods
        function obj = mj_sensor_parser(varargin)
            setProperties(obj,nargin,varargin{:});
        end
    end

    methods (Access=protected)

        function y = stepImpl(~, blank, data)
            names= fieldnames(blank);
            
            dPointer = 1; % next index to write from.
            y = blank;
            for index=1:length(names)
                fieldname = names{index};
                len = length(y.(fieldname));
                y.(fieldname) = data(dPointer:dPointer+len-1);
                dPointer = dPointer + len;
            end
        end

        function out = getOutputSizeImpl(obj)
            out = propagatedInputSize(obj, 1);
        end

        function out = isOutputComplexImpl(obj)
            out = propagatedInputComplexity(obj, 1);
        end

        function out = getOutputDataTypeImpl(obj)
            out = obj.OutputBusName;
        end

        function out = isOutputFixedSizeImpl(obj)
            out = propagatedInputFixedSize(obj, 1);
        end

        function flag = supportsMultipleInstanceImpl(obj)
            % Return true if System block can be used inside a For Each
            % subsystem, which requires multiple object instances
            flag = true;
        end
    end
end
