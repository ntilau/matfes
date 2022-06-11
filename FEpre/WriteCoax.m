function WriteCoax(r1,r2,acc)

% r1 : core radius
% r2 : shell radius
% acc : accuracy

% % % r1 = 1;
% % % r2 = 2;
% % % acc =.5;
nt1 = floor(2*pi*r1/acc);
nt2 = floor(2*pi*r2/acc);
th1 = linspace(0, 2*pi*(nt1-1)/nt1, nt1);
th2 = linspace(0, 2*pi*(nt2-1)/nt2, nt2);

polyID = fopen('FEpre\coax.poly','w');
fprintf(polyID, '%d 2 1 1\n', nt1+nt2);
for i=1:nt1
  fprintf(polyID, '%d %d %d %d\n', i,...
    r1*cos(th1(i)), r1*sin(th1(i)), 0);
end

for i=1:nt2
  fprintf(polyID, '%d %d %d %d\n', nt1+i,...
    r2*cos(th2(i)), r2*sin(th2(i)), 0);
end
%% print edges
nedges = nt1 + nt2;
fprintf(polyID, '%d 1\n', nedges);
for i=1:nt1-1
  fprintf(polyID, '%d %d %d %d\n', i, i+1, i, 1);
end
fprintf(polyID, '%d %d %d %d\n', nt1 , 1, nt1, 1);
for i=nt1+(1:nt2-1)
  fprintf(polyID, '%d %d %d %d\n', i, i+1, i, 2);
end
fprintf(polyID, '%d %d %d %d\n',nt1+nt2, nt1+1, nt1+nt2, 2);
fprintf(polyID, '0\n');
% 
fprintf(polyID, '2\n');
fprintf(polyID, '1 %g %g %d %g\n', 0, 0, 1, acc);
fprintf(polyID, '2 %g %g %d %g\n', (r1+r2)/2, 0, 2, acc);
fclose(polyID);
