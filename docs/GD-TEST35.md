# GD Test 35 — GD Edition dashboard

## Situação anterior

GD Test 34 foi validada no PS4 Fat: o GD Probe NRO abriu, apresentou checksum `ba34abe5f2510398`, teste de 1 MiB PASS, renderizou a tela e registrou entradas do DualShock 4. O log confirmou `the game exited` e `finished: close the app with the PS button`. O frontend anterior intencionalmente não executa teardown do Eden: retornar à biblioteca após um jogo sair continua **não suportado** e o PS deve ser usado para fechar o aplicativo.

## Alterações GD35

- Novo Title ID PS4 `EDPS00035` (instalação lado a lado).
- Build ID `gd-test35-...` via `frontend/frontend.cmake`; **reconfigure o CMake**.
- Home nativa 1920 × 1080, tema azul-marinho e laranja GD, card principal do aplicativo selecionado, painel de sessão anterior e atalhos visuais.
- Biblioteca separada com lista navegável e destaque do item.
- Configuração da fastmem preservada, OFF por padrão; alteração via Quadrado/X e gravação segura em settings.txt.
- Diagnósticos leem `boot.old.log` (histórico real, não telemetria ao vivo).
- Renderização mantida em `sceVideoOut` e nos mesmos dois framebuffers já existentes; nenhuma nova dependência de interface.
- GD Probe NRO 0.1 preservado sem mudanças para não comprometer o teste bem-sucedido da GD34.

## Controles

| Botão | Home / Biblioteca | Configurações | Diagnósticos |
| --- | --- | --- | --- |
| Up/Down | Mudar aplicativo selecionado | Sem ação | Sem ação |
| Right | Ir para biblioteca | Sem ação | Sem ação |
| Left | Retornar à home | Sem ação | Sem ação |
| Cross | Iniciar aplicativo | Alternar fastmem | Sem ação |
| Square | Configurações | Home | Configurações |
| Triangle | Diagnósticos | Diagnósticos | Home |
| Circle | Voltar à home | Home | Home |

Os arquivos existentes `game.txt` e `nomenu.txt` continuam com precedência no launcher. O GD Probe é incluído no PKG somente se `switch-probe/gd-probe.nro` existir; caso não exista, o hbmenu permanece como fallback.

## Compilar no Linux Mint

```bash
cd ~/nx-on-orbis
git status --short
git fetch origin
git switch --track origin/dev/gd-test35
export DEVKITPRO=/opt/devkitpro
export DEVKITA64=$DEVKITPRO/devkitA64
export PATH=$DEVKITA64/bin:$DEVKITPRO/tools/bin:$PATH
make -C switch-probe
source tools/env-build.sh
unset OO_PS4_TOOLCHAIN
bash configure-eden.sh
cmake --build build-eden --target eden-ps4 -j4
grep -aEom3 'gd-test35|GD EDITION  /  TEST 35' build-eden/bin/eden-ps4
bash package-eden.sh
ls -lh out-eden/*EDPS00035*.pkg
```

O usuário compilou e empacotou localmente a GD35 em 2026-10-09, gerando `IV0000-EDPS00035_00-EDENPS4000000000.pkg` (~58 MB) e preservando o ELF correspondente. Capturas do PS4 Fat confirmam funcionamento inicial da interface e execução do GD Probe; ver registro de validação abaixo. Isso não equivale a uma certificação de estabilidade ou compatibilidade com jogos comerciais.

## Testes no console

1. Instalar GD35 sem remover GD34, garantindo Title ID EDPS00035.
2. Conferir home, seleção do GD Probe, navegação para biblioteca.
3. Abrir Quadrado e Triângulo; confirmar histórico de FPS da sessão anterior e fastmem OFF.
4. Iniciar GD Probe; validar CPU, memória, botões e saída por Options. A tela fica congelada após o exit por design do frontend; fechar pelo botão PS.
5. Ler boot.log e verificar `gd-test35`, mensagens de erro e o encerramento.

## Registro de teste real no PS4 Fat — 2026-10-09

Capturas de tela enviadas pelo testador mostram:

- Home da GD Test 35 carregada com destaque do GD Probe, barra de fastmem em modo seguro e cards da biblioteca/diagnósticos.
- Biblioteca com dois aplicativos visíveis: GD Probe e Homebrew Menu.
- Configurações acessíveis, fastmem exibida como DESATIVADA / RECOMENDADA.
- Diagnósticos exibindo dados da sessão anterior, com build `gd-test34-20261010T021841Z-0498b1e860c4`, FPS reportado 38.2, speed 100%, frame 26.1 ms, fastmem off (settings).
- GD Probe 0.1 iniciou, exibiu checksum de CPU `ba34abe5f2510398`, `Memory 1 MiB: PASS`, contagem de apresentação e eventos de botões (last input UP).

Os 38.2 FPS são históricos da execução da GD34, não uma medida da home GD35. Não comparar diretamente com ~60 FPS do hbmenu (cargas distintas). Ainda pendente: log de runtime da GD35, validação do encerramento específico desta versão e testes de jogos comerciais.

## Validacao adicional — boot.log GD35 no PS4 Fat

O testador confirmou pelo log:

- `gd-test35-20261010T023255Z-0498b1e860c4`.
- `menu choice 1 of 2: /app0/assets/misc/gd-probe.nro` aos 76.108 s.
- `fastmem: off (settings)`.
- Status de jogo: 16.7 FPS (~87 s), 35.9 FPS (~97 s) e 38.2 FPS (~107 s), sempre speed reportada 100%; ultimo frame 26.2 ms.
- Guest + tabelas: 65 MiB; maior bloco direto livre: 1982 MiB.
- `the game exited` aos 115.932 s, seguido de `finished: close the app with the PS button` aos 115.939 s.
- Nenhum crash encontrado no trecho filtrado apresentado.

Comparacao: o GD Probe tambem reportou 38.2 FPS na GD34. Portanto, este teste nao evidencia regressao nesta carga; isso nao equivale a benchmark de jogos comerciais ou do desempenho da home.
