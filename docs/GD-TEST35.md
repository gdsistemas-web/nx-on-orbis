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

O pacote só está pronto para distribuição após build e teste de hardware. **Esta branch ainda não foi compilada nem validada no console durante a edição remota.**

## Testes no console

1. Instalar GD35 sem remover GD34, garantindo Title ID EDPS00035.
2. Conferir home, seleção do GD Probe, navegação para biblioteca.
3. Abrir Quadrado e Triângulo; confirmar histórico de FPS da sessão anterior e fastmem OFF.
4. Iniciar GD Probe; validar CPU, memória, botões e saída por Options. A tela fica congelada após o exit por design do frontend; fechar pelo botão PS.
5. Ler boot.log e verificar `gd-test35`, mensagens de erro e o encerramento.
