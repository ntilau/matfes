clear all; clc;
close all
Config();
format short
% warning('off','all');
model = 1;
HarmsRetained = 4;
Sys.pOrd = 3;
Sys.hOrd = 1;
switch model
    case 1
        prjName = 'CircKoshiba26';
    case 2
        prjName = 'CircKoshiba26_5';
end
Mesh = IOrPoly( prjName, 'q34aAQ', Sys.hOrd, 1e-3);
% PlotMesh(Mesh,1)
% PlotPoly(prjName, figure)
Sys.WPnModes = 1;
Sys.WPportPlot = 1;
Sys.WPmodePlot = 1;
Sys.Height = 22.86e-03/2;
Sys.WPpow = 1;
% Sys.Einc = 1e3;%3.080e+03 * sqrt(22.86e-03/2);
Mesh.mur = [1 1];
Mesh.kr = [0 0];
switch model
    case 1
        Mesh.epsr = [1 11.7];
    case 2
        Mesh.epsr = [1 1 1 1 11.7];
end
Mesh.BC.Dir = 1;
Mesh.BC.WP = [11 12 13];
Mesh.NLlab = length(Mesh.epsr);
%%% ferrite
Ferr.Gamma = 1.759e7; %[C/kg]
Ferr.Ms = 1317; % Oe
Ferr.H0 = 200; % G
Ferr.dH = 135; % Oe*s
Ferr.w0 = Ferr.Gamma*Ferr.H0;
Ferr.wm = Ferr.Gamma*Ferr.Ms;
Ferr.aDH = Ferr.Gamma*Ferr.dH/2;
Ferr.alpha = 1;
Mesh.Ferr = Ferr;
f1 = 1;
f2 = 1.1;
Sys.HBharms = [f1 f2 2*f1-f2 2*f2-f1 3*f1 3*f2  2*f1+f2  2*f2+f1];
Sys.HBharms = Sys.HBharms(1:HarmsRetained);
Sys.HBharmPlot = length(Sys.HBharms);
Sys.OverSampling = 2;
Sys.SinOnly=true;
Sys.nHarms = length(Sys.HBharms);
if ~isfield(Sys,'SinOnly')
    Sys.nHarms = length(Sys.HBharms)*2;
else
    if ~Sys.SinOnly
        % have included cos harmonics
        Sys.HBharmPlot = 2*Sys.HBharmPlot-1;
    end
end
% intp=1;
% inth=1;
%%
Mesh.BC.DDschur = 2;
[Mesh, Sys] = GetBndMap(Sys, Mesh);

Mesh.MurMat = zeros(Sys.nHarms*Sys.nHarms,Mesh.NELE);
Mesh.KrMat = zeros(Sys.nHarms*Sys.nHarms,Mesh.NELE);
idxNL = find(Mesh.elab == Mesh.NLlab);

Sys = CalcDoFsNumber(Sys, Mesh);
Sys.u = zeros(Sys.NDOFs*Sys.nHarms,1);

Sys.Pfund = 10;
Sys.Pitrf = 100;
Sys.WPnum = 6;
Sys.Power = ones(1,length(Mesh.BC.WP)*Sys.WPnModes*Sys.nHarms);
Sys.Power(1) = sqrt(Sys.Pfund);
Sys.Power(Sys.WPnum) = sqrt(Sys.Pitrf);
% Sys.NLlab = Mesh.NLlab;

%%%
Sys.Compute = ones(1,length(Sys.RegDoF));
Sys.nFreqs = 1;
Sys.freqs = 10e9;
% Sys.nFreqs = 41;
% Sys.freqs = linspace(8e9,12e9,Sys.nFreqs);
Sys.Sparams = zeros(length(Mesh.BC.WP)*Sys.WPnModes*Sys.nHarms, Sys.nFreqs);
for kf = 1:Sys.nFreqs
    Sys.freq = Sys.freqs(kf);
    
    f1 = Sys.freq;
    f2 = 1.1e10;
    if f1 == f2
        continue;
    end
    Sys.HBharms = [f1 f2 2*f1-f2 2*f2-f1 3*f1 3*f2  2*f1+f2  2*f2+f1]/f1;
    Sys.HBharms = Sys.HBharms(1:HarmsRetained);
    
    fprintf('freq = %g GHz\n',Sys.freq/1e9);
    error = 1;
    Sys.u0 = Sys.u;
    if ~isfield(Sys,'SinOnly')
        omega(1:2:2*length(Sys.HBharms),1) = 2*pi*Sys.freq*Sys.HBharms.';
        omega(2:2:2*length(Sys.HBharms),1) = 2*pi*Sys.freq*Sys.HBharms.';
    else
        omega = 2*pi*Sys.freq*Sys.HBharms.';
    end
    
    mur = diag(1 + ((Ferr.w0*ones(Sys.nHarms,1))+1i.*Ferr.aDH).*...
        (Ferr.wm*ones(Sys.nHarms,1))./...
        ((Ferr.w0*ones(Sys.nHarms,1)+1i.*Ferr.aDH).^2-(omega).^2));
    kr = diag(omega.*(Ferr.wm*ones(Sys.nHarms,1))./...
        ((Ferr.w0*ones(Sys.nHarms,1)+1i.*Ferr.aDH).^2-(omega).^2));
    Mesh.MurMat(:,idxNL) = mur(:)*ones(1,length(idxNL));
    Mesh.KrMat(:,idxNL) =  kr(:)*ones(1,length(idxNL));
    
    %%% copying for full assemb + solve + recover fields
    SysF = Sys;
    MeshF = Mesh;
    
    tD = 0;
    tF = 0;
    
    while error > 1e-9
        
        %%% FULL
        ttF = tic;
        [SysF,MeshF] = AssembHBFerrite(SysF, MeshF);
        SysF =  AssembWPHB(SysF);      
        XF =  SolvDir(SysF);
        SysF.u = zeros(SysF.NDOFs*SysF.nHarms,1);
        SysF.u(SysF.nnWP) = XF(length(SysF.WP)*SysF.WPnModes*SysF.nHarms+1:end, 1);
        for jh=1:length(SysF.Harms)
            for ip=1:length(SysF.WP)
                SysF.u((jh-1)*SysF.NDOFs+SysF.WP{ip}) = SysF.WPgvec{ip,jh}*...
                    (XF((1:SysF.WPnModes)+((jh-1)+(ip-1)*SysF.nHarms)*SysF.WPnModes,1).*...
                    Sys.Power((1:SysF.WPnModes)+((jh-1)+(ip-1)*SysF.nHarms)*SysF.WPnModes).');
            end
        end
        errorF = norm(SysF.u - SysF.u0) / norm(SysF.u);
        SysF.u0 = SysF.u;
        spF = XF(1:length(SysF.WP)*SysF.WPnModes*SysF.nHarms,1);
        spF(1,1) = (spF(1,1) - 1) * sqrt(SysF.Pfund);
        spF(SysF.WPnum,1) = (spF(SysF.WPnum,1) - 1) * sqrt(SysF.Pitrf);
        tmp = toc(ttF);
%         fprintf('%2.6e s\t\t', tmp);
        tF = tF + tmp;
        
        %%% DD
        ttD = tic;
        if ~isfield(Sys,'first')
            Sys.RegToCompute = 1:Mesh.NLlab-1;
            [Sys,Mesh] = AssembHBDDFerrite(Sys, Mesh);
            Sys.Slin = Sys.S;
            Sys.Tlin = Sys.T;
            Sys.first = true;
        end
        Sys.RegToCompute = Mesh.NLlab;
        [Sys,Mesh] = AssembHBDDFerrite(Sys, Mesh);
        Sys.S = Sys.S + Sys.Slin;
        Sys.T = Sys.T + Sys.Tlin;
        Sys =  AssembWPHBDDschur(Sys);
        [XD, Sys] =  SolvHBDDschur(Sys);
        Sys.Compute =  Sys.Compute*0;
        Sys.Compute(Mesh.NLlab) = 1;
        Sys.u = zeros(Sys.NDOFs*Sys.nHarms,1);
        Sys.u(Sys.nnWP(Sys.BndDoFRed(length(SysF.WP)*SysF.WPnModes*SysF.nHarms+1:end)-length(SysF.WP)*SysF.WPnModes*SysF.nHarms)) = XD(length(SysF.WP)*SysF.WPnModes*SysF.nHarms+1:end,1);
        for jh=1:length(Sys.Harms)
            for ip=1:length(Sys.WP)
                Sys.u((jh-1)*Sys.NDOFs+Sys.WP{ip}) = Sys.WPgvec{ip,jh}*...
                    XD((1:Sys.WPnModes)+((jh-1)+(ip-1)*Sys.nHarms)*Sys.WPnModes,1);
            end
        end
        X5 = Sys.AII{Mesh.NLlab}\(-Sys.AIF{Mesh.NLlab}*XD(:,1));
        Sys.u(Sys.nnWP(Sys.RegDoFRed{Mesh.NLlab}-length(SysF.WP)*SysF.WPnModes*SysF.nHarms)) = X5(:,1);
        error = norm(Sys.u - Sys.u0) / norm(Sys.u);
        Sys.u0 = Sys.u;
        spD = XD(1:length(Sys.WP)*Sys.WPnModes*Sys.nHarms,1);
        spD(1,1) = (spD(1,1) - 1) * sqrt(Sys.Pfund);
        spD(Sys.WPnum,1) = (spD(Sys.WPnum,1) - 1) * sqrt(Sys.Pitrf);
        tmp = toc(ttD);
%         fprintf('%2.6e s\n', tmp);
        tD = tD + tmp;
                    
%         fprintf('Sparam error = %2.6g\n',norm(spF-spD))    
        Sys.Sparams(:,kf) = spF(:,1);
        fprintf('%2.6e\t\t%2.6e\n',error,errorF);
%         break
    end
    if Sys.freq == 10e9
        Sys.uPlot = SysF.u;
    end
    
end

Acc = tF/tD;
fprintf('Acc = %2.4g\n',Acc);

% return
%%
idx = find(Sys.freqs == f2);
Sys.freqs(idx) = [];
Sys.Sparams(:,idx) = [];
%%% Scattering
if Sys.nFreqs == 1
    PowDist = Sys.db(Sys.Sparams(Sys.WPmodePlot:Sys.WPnModes:end,1));
else
    figure; plot(Sys.freqs,Sys.db(Sys.Sparams(Sys.WPmodePlot:Sys.WPnModes:end,:).'));
end
% disp(max(PowDist((4+(1:2)))) - max(PowDist(end-1:end)));
% return
%% Field
for ih=1:Sys.HBharmPlot
    Sys.u = Sys.uPlot((ih-1)*SysF.NDOFs+(1:SysF.NDOFs));
%     Sys.u = Sys.u(Sys.DDmap);
    if exist('pdeplot','file')
        figure; pdeplot(Mesh.refNode.',[],Mesh.refEle.',...
            'xydata',(abs(Sys.u)),...
            'mesh','off',...
            'colormap','jet','xygrid','off');
        axis equal; axis tight; camlight left; lighting phong;
    else
        IOwVTK(Sys, Mesh, prjName);
    end
end