%% Plantilla futura para estimacion AoA con matriz de antenas
% Este script es conceptual. No debe usarse para obtener un angulo real
% con los datos actuales, porque no hay matriz de antenas conectada.
%
% Cuando se disponga de una matriz real, las muestras I/Q deberán separarse
% por antena según el patrón de conmutación utilizado durante el CTE.

clear; clc; close all;

%% Parametros del sistema

M = 4;              % Numero de antenas del array
fc = 2.44e9;        % Frecuencia aproximada BLE
c = 3e8;            % Velocidad de la luz
lambda = c/fc;      % Longitud de onda
d = lambda/2;       % Separacion habitual entre antenas

%% Entrada esperada

% En una prueba real, I y Q vendrian del log capturado durante el CTE.
% Aqui se asume que ya existen los vectores I y Q cargados en MATLAB.
%
% z[n] = I[n] + jQ[n]

z = I + 1j*Q;

%% Descartar tramo inicial

% Las primeras muestras pueden corresponder al periodo de referencia o a un
% transitorio inicial. Este valor debera ajustarse con la configuracion real.

startIdx = 9;
z_use = z(startIdx:end);

%% Separacion de muestras por antena

% Ejemplo simple con patron repetido:
% A1, A2, A3, A4, A1, A2, A3, A4...
%
% En una implementacion real, esta parte debera adaptarse a ant_patterns[]
% y al orden fisico de las antenas en la matriz.

antSamples = cell(M,1);

for n = 1:length(z_use)
    ant = mod(n-1, M) + 1;
    antSamples{ant}(end+1,1) = z_use(n);
end

%% Calculo de fase media por antena

phaseAnt = zeros(M,1);

for m = 1:M
    phaseAnt(m) = angle(mean(antSamples{m}));
end

phaseAnt_unwrap = unwrap(phaseAnt);

%% Diferencia de fase entre antenas consecutivas

deltaPhi = diff(phaseAnt_unwrap);
deltaPhiMean = mean(deltaPhi);

%% Estimacion del angulo

theta_rad = asin((deltaPhiMean * lambda) / (2*pi*d));
theta_deg = rad2deg(theta_rad);

fprintf("Angulo AoA estimado: %.2f grados\n", theta_deg);

%% Figuras

figure;
plot(1:M, phaseAnt_unwrap, 'o-', 'LineWidth', 1.2);
grid on;
xlabel('Antena');
ylabel('Fase media unwrap (rad)');
title('Fase media por antena');

figure;
bar(deltaPhi);
grid on;
xlabel('Par de antenas consecutivas');
ylabel('\Delta\phi (rad)');
title('Diferencia de fase entre antenas');