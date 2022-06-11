function WriteWaveGuide(a,L)


polyID = fopen('FEpre\WaveGuide.poly','w');
fprintf(polyID, '%d 2 1 1\n', 4);

fprintf(polyID, '1 %g %g 0 %d\n', -L/2, a/2, 11);
fprintf(polyID, '2 %g %g 0 %d\n', L/2, a/2, 12);
fprintf(polyID, '3 %g %g 0 %d\n', L/2, -a/2, 12);
fprintf(polyID, '4 %g %g 0 %d\n', -L/2, -a/2, 11);

%%% print edges
fprintf(polyID, '%d 1\n', 4);

fprintf(polyID, '1 %d %d %d\n', 1, 2, 1);
fprintf(polyID, '2 %d %d %d\n', 3, 2, 12);
fprintf(polyID, '3 %d %d %d\n', 4, 3, 1);
fprintf(polyID, '4 %d %d %d\n', 1, 4, 11);

fprintf(polyID, '0\n');

fprintf(polyID, '1\n');
fprintf(polyID, '1 %g %g %d\n', 0, 0, 1);

fclose(polyID);