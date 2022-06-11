clear all; %clc;
close all
Config();
Sys.pOrd = 2;
Sys.hOrd = 1;
a = 19.05;
b = a/2;
r = 0.05*a;
c = 0.4*a;
L = 1.4*a;
EpsR = 112.5;
nt = 60;
WriteTwoPosts(a,c,r,L,nt);
prjName = 'TwoPosts';
Mesh = IOrPoly(prjName, 'q34a1A', Sys.hOrd, 1e-3);
prjName = [ prjName, 'HB'];
Sys.WPnModes = 5;
Sys.WPportPlot = 1;
Sys.WPmodePlot = 1;
Sys.Height = b*1e-3;
Sys.WPpow = 1;
% Sys.Einc = 3.416151024952731e+04 * sqrt(b*1e-3); % 100 W
Mesh.epsr = [1 EpsR];% [1 2.1*(1-1i*0.02)];
Mesh.kerr = [0 1.625e-10];
Mesh.NLlab = 2;
Mesh.BC.Dir = 1;
Mesh.BC.WP = [11 12];
Sys.HBharmPlot = 2;
Sys.HBharms = [1 3];
% Sys.Sin=false;
Sys.nHarms = length(Sys.HBharms);
if ~isfield(Sys,'Sin')
    Sys.nHarms = length(Sys.HBharms)*2;
else
    if Sys.Sin
        Sys.HBharmPlot = 2*Sys.HBharmPlot-1;
    end
end
% PlotMesh(Mesh,1)

Sys = CalcDoFsNumber(Sys, Mesh);
Sys.u = zeros(Sys.NDOFs*Sys.nHarms,1);

%%
fc = 7.868577546294576e+09;
Sys.nFreqs = 1;
Sys.freqs = 11.2e9;
Sys.fPlot = Sys.freqs(1);
% Sys.nFreqs = 81;
% Sys.freqs = linspace(1.38*fc, 1.48*fc, Sys.nFreqs);
Sys.Sparams = zeros(length(Mesh.BC.WP)*Sys.WPnModes*Sys.nHarms, Sys.nFreqs);
for kf = 1:Sys.nFreqs
    Sys.freq = Sys.freqs(kf);
    fprintf('freq = %g GHz\n',Sys.freq/1e9);
    error = 1;
    Sys.u0 = Sys.u;
    while error > 1e-4
        [Sys,Mesh] = AssembHBKerr(Sys, Mesh);
        Sys =  AssembWPHB(Sys);
%         Sys.B(:,1) = Sys.B(:,1) + Sys.B(:,Sys.WPnModes+1);
        X = Sys.A\Sys.B;

        sp = X(1:length(Sys.WP)*Sys.WPnModes*Sys.nHarms,...
            1:length(Sys.WP)*Sys.WPnModes*Sys.nHarms) - ...
            eye(length(Sys.WP)*Sys.WPnModes*Sys.nHarms);
        Sys.Sparams(:,kf) = ...
            sp(:,(Sys.WPportPlot-1)*Sys.WPnModes*Sys.nHarms+...
            Sys.WPmodePlot);
        fprintf('  losses = %2.2g%%\n', abs(1 - norm(Sys.Sparams(:,kf)))*100);
        
        Sys.u = zeros(Sys.NDOFs*Sys.nHarms,1);
        Sys.u(Sys.nnWP) = X(length(Sys.WP)*Sys.WPnModes*Sys.nHarms+1:end, ...
            (Sys.WPportPlot-1)*Sys.WPnModes+Sys.WPmodePlot);
        for jh=1:length(Sys.Harms)
            for ip=1:length(Sys.WP)
                Sys.u((jh-1)*Sys.NDOFs+Sys.WP{ip}) = ...
                    Sys.WPgvec{ip,jh}*X((1:Sys.WPnModes)+((jh-1)+(ip-1)*Sys.nHarms)*Sys.WPnModes, ...
                    (Sys.WPportPlot-1)*Sys.WPnModes+Sys.WPmodePlot);
            end
        end
        error = norm(Sys.u - Sys.u0) / norm(Sys.u);
        Sys.u0 = Sys.u;
        fprintf(' %g\n',error);
    end
    if Sys.freq == Sys.fPlot
        Sys.uPlot = Sys.u;
    end
end

%%% Scattering
if Sys.nFreqs == 1
    Sys.db(Sys.Sparams(Sys.WPmodePlot:Sys.WPnModes:end,:))
else
    figure; plot(Sys.freqs,Sys.db(Sys.Sparams(Sys.WPmodePlot:Sys.WPnModes:end,:).'));
end

%%% Field
Sys.HBharmPlot =2 ;
Sys.u = Sys.uPlot((Sys.HBharmPlot-1)*Sys.NDOFs+(1:Sys.NDOFs));
if exist('pdeplot','file')
    figure; pdeplot(Mesh.refNode.',[],Mesh.refEle.',...
        'xydata',(abs(Sys.u)),...
        'mesh','off',...
        'colormap','jet','xygrid','off');
    axis equal; axis tight; camlight left; lighting phong;
else
    IOwVTK(Sys, Mesh, prjName);
end

max(abs(Sys.u))