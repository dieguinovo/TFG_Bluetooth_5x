%% Analisis de muestras I/Q capturadas durante el CTE
% Este script procesa el log obtenido desde la placa nRF5340-DK receptora.
% El objetivo es extraer los bloques CTE, leer sus muestras I/Q y generar
% varias graficas para comprobar que la recepcion del CTE es coherente.

clear; clc; close all;

%% 1. Lectura del archivo de log

% Nombre del archivo de log (debe estar en la misma carpeta que este script)
fname = "log.txt";

% Comprobamos que el archivo existe antes de intentar leerlo.
if ~isfile(fname)
    error("No se encuentra el archivo %s en la carpeta actual", fname);
end

% Leemos todo el contenido del archivo como texto.
txt = fileread(fname);

%% 2. Busqueda de bloques CTE completos

% El log contiene bloques con esta estructura:
%
% CTE[0]: samples=45, type=AOA, slot=2 us, status=CRC OK, RSSI=-46.0 dBm
% idx,I,Q
% 0,...
% 1,...
% ...
% VALID_SAMPLES=45
% END_IQ
%
% La expresion regular siguiente busca bloques completos desde la linea CTE
% hasta END_IQ y extrae los datos importantes.

expr = "CTE\[\d+\]: samples=(\d+), type=([A-Z0-9_]+), slot=(\d+) us, status=([^,]+), RSSI=(-?\d+)\.(\d) dBm\s*idx,I,Q\s*(.*?)VALID_SAMPLES=(\d+)\s*END_IQ";

% regexp busca todos los bloques que cumplen ese formato.
% La opcion "tokens" hace que se guarden las partes capturadas entre parentesis.
tokens = regexp(txt, expr, "tokens");

% Numero total de bloques CTE encontrados.
nBlocks = numel(tokens);

% Si no se encuentra ningun bloque, se detiene el script.
if nBlocks == 0
    error("No se han encontrado bloques CTE completos en el log.");
end

%% 3. Inicializacion de variables

% Cada bloque tendra una tabla con sus muestras: indice, I y Q.
blocks = cell(nBlocks,1);

% RSSI de cada bloque, en dBm.
RSSI = zeros(nBlocks,1);

% Duracion del slot de muestreo, en microsegundos.
slot_us = zeros(nBlocks,1);

% Numero total de muestras indicadas por el log.
sampleCount = zeros(nBlocks,1);

% Numero de muestras validas.
validSamples = zeros(nBlocks,1);

% Metrica de estabilidad de fase: desviacion estandar de los incrementos de fase.
phaseNoise = zeros(nBlocks,1);

% Offset de frecuencia estimado a partir de la pendiente de la fase.
f_est = zeros(nBlocks,1);

%% 4. Procesado de cada bloque CTE

for k = 1:nBlocks

    % tokens{k} contiene todos los datos extraidos del bloque k.
    t = tokens{k};

    % Numero de muestras del CTE (45). 
    sampleCount(k) = str2double(t{1});

    % Tipo de CTE,(AoA).
    cteType = t{2}; %#ok<NASGU>

    % Slot de muestreo en microsegundos (2 us).
    slot_us(k) = str2double(t{3});

    % Estado del paquete, por ejemplo CRC OK.
    status = t{4}; %#ok<NASGU>

    % RSSI recibido.
    % El log lo separa como parte entera y decimal, por ejemplo -46 y 0.
    % Lo reconstruimos como -46.0.
    RSSI(k) = str2double([t{5} '.' t{6}]);

    % Numero de muestras validas del bloque.
    validSamples(k) = str2double(t{8});

    % t{7} contiene todas las lineas de datos:
    % 0,I,Q
    % 1,I,Q
    % ...
    data_txt = splitlines(strtrim(t{7}));

    % Vectores donde guardaremos las muestras de este bloque.
    idx = [];
    I = [];
    Q = [];

    % Recorremos linea por linea las muestras del bloque.
    for i = 1:numel(data_txt)

        % Eliminamos espacios al principio y al final de la linea.
        line = strtrim(data_txt{i});

        % Buscamos lineas con formato:
        % indice,I,Q
        % Ejemplo:
        % 0,-52,13
        %
        % Se permiten valores negativos para I y Q.
        values = regexp(line, "^(\d+),(-?\d+),(-?\d+)$", "tokens");

        % Si la linea tiene el formato correcto, guardamos los tres valores.
        if ~isempty(values)

            values = values{1};

            % Indice de muestra.
            idx(end+1,1) = str2double(values{1});

            % Componente en fase.
            I(end+1,1) = str2double(values{2});

            % Componente en cuadratura.
            Q(end+1,1) = str2double(values{3});
        end
    end

    % Guardamos las muestras de este bloque en una tabla.
    blocks{k} = table(idx,I,Q);

    %% Calculo de fase para este bloque

    % Cada muestra I/Q representa una muestra compleja:
    %
    % s[n] = I[n] + jQ[n]
    %
    % La fase de cada muestra se calcula como el angulo del vector complejo.
    % atan2(Q,I) da el angulo correcto teniendo en cuenta el cuadrante.
    %
    % unwrap elimina saltos artificiales de +pi a -pi.
    phi = unwrap(atan2(Q,I));

    % Diferencia de fase entre muestras consecutivas.
    dphi = diff(phi);

    % Tiempo entre muestras.
    % slot_us esta en microsegundos, por eso se pasa a segundos.
    Ts = slot_us(k)*1e-6;

    % Metrica de estabilidad de fase.
    % std(dphi) mide cuanto varian los saltos de fase entre muestras.
    phaseNoise(k) = std(dphi);

    % Estimacion del offset de frecuencia.
    %
    % Si la fase tiene una pendiente, esa pendiente equivale a una frecuencia
    % residual en banda base entre transmisor y receptor.
    %
    % Relacion:
    % dphi = 2*pi*f*Ts
    %
    % Despejando:
    % f = mean(dphi)/(2*pi*Ts)
    %
    % Este valor NO es la frecuencia Bluetooth, sino un pequeno offset residual.
    f_est(k) = mean(dphi)/(2*pi*Ts);
end

%% 5. Eleccion de un bloque representativo

% Elegimos el primer bloque con el mayor numero de muestras validas.
bestK = find(validSamples == max(validSamples), 1, "first");

% Extraemos la tabla del bloque elegido.
T = blocks{bestK};

% Extraemos sus columnas.
idx = T.idx;
I = T.I;
Q = T.Q;

%% 6. Calculos sobre el bloque elegido

% Amplitud de cada muestra compleja:
%
% A[n] = sqrt(I[n]^2 + Q[n]^2)
%
% Indica el modulo del vector complejo I/Q.
A = sqrt(I.^2 + Q.^2);

% Fase del bloque elegido.
phi = unwrap(atan2(Q,I));

%% 7. Resumen por consola

fprintf("Bloques encontrados: %d\n", nBlocks);
fprintf("Bloque elegido: %d\n", bestK);
fprintf("RSSI bloque elegido: %.1f dBm\n", RSSI(bestK));
fprintf("Muestras validas: %d/%d\n", validSamples(bestK), sampleCount(bestK));
fprintf("Slot: %d us\n", slot_us(bestK));
fprintf("f_est: %.1f Hz\n", f_est(bestK));
fprintf("sigma_dphi: %.3f rad\n", phaseNoise(bestK));

%% 8. Figura 1: Plano I-Q

% Esta grafica representa las muestras como puntos (I,Q) en el plano complejo.
%
% Durante el CTE se transmite un tono constante. En condiciones ideales,
% un tono de este tipo genera una trayectoria circular en el plano I-Q.
%
% En hardware real no se espera un circulo perfecto debido a ruido,
% cuantizacion, offset de frecuencia, offset DC y desbalance entre I y Q.

figure;
scatter(I,Q,35,'filled');
grid on;
axis equal;

xlabel('Fase I (LSB)');
ylabel('Cuadratura Q (LSB)');

%title(sprintf('Plano I-Q (bloque #%d) | RSSI = %.1f dBm | VALID = %d/%d', ...
    %bestK, RSSI(bestK), validSamples(bestK), sampleCount(bestK)));

%% 9. Figura 2: Fase instantanea

% La fase instantanea se obtiene a partir de atan2(Q,I).
%
% Si existe un pequeño offset de frecuencia entre transmisor y receptor,
% la fase no permanece constante, sino que evoluciona con una pendiente.
%
% El tramo inicial puede ser menos regular por la transicion entre el paquete
% BLE y el CTE, muestras de referencia iniciales, efectos del unwrap o
% pequenas imperfecciones del receptor.

figure;
plot(idx,phi,'o-','LineWidth',1.2);
grid on;

xlabel('Índice de muestra');
ylabel('Fase unwrap \phi (rad)');

%title(sprintf('Fase instantánea (unwrap) | f_{est} = %.1f Hz', f_est(bestK)));

%% 10. Figura 3: Amplitud de las muestras

% La amplitud se calcula como:
%
% A[n] = sqrt(I[n]^2 + Q[n]^2)
%
% Como el CTE es un tono constante, la amplitud deberia mantenerse en un
% rango aproximadamente estable. No tiene que ser totalmente plana porque
% hay ruido, cuantizacion y pequeñas variaciones del canal.

figure;
plot(idx,A,'o-','LineWidth',1.2);
grid on;

xlabel('Índice de muestra');
ylabel('Amplitud (LSB)');
%title('Amplitud de las muestras I/Q durante el CTE');

%% 11. Figura 4: Evolucion por bloques

% Esta grafica analiza todos los bloques CTE capturados.
%
% En el eje izquierdo se representa el RSSI de cada bloque.
% En el eje derecho se representa la estabilidad de fase sigma_dphi.
%
% Sirve para comprobar que el resultado no pertenece a un unico bloque aislado,
% sino que la recepcion del CTE es repetible en el tiempo.

figure;

yyaxis left;
plot(1:nBlocks, RSSI, 'o-','LineWidth',1.2);
ylabel('RSSI (dBm)');

yyaxis right;
plot(1:nBlocks, phaseNoise, 's-','LineWidth',1.2);
ylabel('\sigma_{\Delta\phi} (rad)');

grid on;
xlabel('Bloque CTE');
%title('Evolución por bloques: RSSI y estabilidad de fase');

%% 12. Tabla resumen

% Creamos una tabla con una fila por cada bloque CTE.
%
% Columnas:
% - Bloque: numero de bloque
% - RSSI_dBm: potencia recibida
% - Slot_us: tiempo entre muestras
% - SampleCount: numero total de muestras
% - ValidSamples: numero de muestras validas
% - PhaseNoise_std_dphi: estabilidad de fase
% - f_est_Hz: offset de frecuencia estimado

Summary = table((1:nBlocks)', RSSI, slot_us, sampleCount, validSamples, phaseNoise, f_est, ...
    'VariableNames', {'Bloque','RSSI_dBm','Slot_us','SampleCount','ValidSamples','PhaseNoise_std_dphi','f_est_Hz'});

disp("=== Resumen por bloque ===");
disp(Summary);