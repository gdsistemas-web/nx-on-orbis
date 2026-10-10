# GD Test 36 — GD Probe 0.2 (experimental)

## Baseline validado

GD Test 35 foi testada no PS4 Fat: home, biblioteca, configurações, diagnósticos e GD Probe 0.1 abriram. No GD Probe, o log registrou 38,2 FPS, velocidade de emulação de 100%, 26,2 ms/quadro, fastmem OFF, 65 MiB de memória lazy comprometida e encerramento confirmado por `the game exited`. Esses números são do teste da GD35, não benchmarks de jogos comerciais.

## Novidades

- Branch `dev/gd-test36`, novo Title ID `EDPS00036`, build ID `gd-test36`.
- Preserva a home/dashboard, o leitor de `boot.old.log`, fastmem desligada por padrão e o layout visual GD.
- `switch-probe/source/main.c` agora implementa **GD Probe 0.2** com tela texto segura e modo gráfico 1280 × 720 RGBA8888, quatro cores e animação de quadrado.
- A rotina de CPU e os testes de memória de 1 MiB e 4 MiB são executados ao iniciar.
- Contador de loops e taxa calculada a cada segundo via relógio monotônico. **Essa taxa não é FPS da GPU ou do emulador**; comparar com o `status: game` do boot.log.
- A entra no modo gráfico; B retorna ao modo texto. Direcionais esquerda/direita ajustam a velocidade da animação. +/Options encerra o NRO; o frontend PS4 continua aguardando fechamento com PS.
- Usa API de framebuffer do libnx baseada no exemplo oficial `switchbrew/switch-examples/graphics/simplegfx`.
- Se `framebufferCreate` falhar, volta ao console e exibe a falha.

**Risco experimental:** a alternância entre console e framebuffer depende da implementação HLE de display do Eden e ainda não foi validada. Começar pelo teste texto. Se a tela ficar preta ou travar ao pressionar A, fechar o aplicativo pelo botão PS, recuperar o log e manter GD35 como fallback. O NRO não é prova de compatibilidade com jogos comerciais.

## Build no Linux Mint

```bash
cd ~/nx-on-orbis
git status --short
git fetch origin
git switch -c dev/gd-test36 --track origin/dev/gd-test36
export DEVKITPRO=/opt/devkitpro
export DEVKITA64=$DEVKITPRO/devkitA64
export PATH=$DEVKITA64/bin:$DEVKITPRO/tools/bin:$PATH
make -C switch-probe clean
make -C switch-probe
source tools/env-build.sh
unset OO_PS4_TOOLCHAIN
bash configure-eden.sh
cmake --build build-eden --target eden-ps4 -j4
grep -aEom3 'gd-test36|GD EDITION  /  TEST 36' build-eden/bin/eden-ps4
bash package-eden.sh
ls -lh out-eden/*EDPS00036*.pkg
```

Verificar no empacotamento a mensagem `including built-in GD Probe`. O PKG foi compilado pelo testador no Linux Mint, gerando `IV0000-EDPS00036_00-EDENPS4000000000.pkg` (~58 MB) e ELF `eden-ps4-gd-test36-20261009-2344.elf` (~70 MB). O teste de imagens no PS4 Fat ocorreu depois; preservar os ELFs.

## Roteiro de testes

1. Instalar GD36 separadamente, sem remover GD35.
2. Conferir home e configurações.
3. Abrir GD Probe 0.2 e confirmar checksum, Memory 1 MiB PASS e Memory 4 MiB PASS.
4. Apertar A (círculo no mapeamento de Switch do frontend) para modo gráfico. Conferir quatro faixas coloridas e um quadrado em movimento.
5. Direcional esquerda/direita varia a velocidade; B (X físico, no layout mapeado) retorna ao texto.
6. Options/+ encerra; aguardar confirmação `the game exited` e `finished` no `boot.log`.
7. Comparar log completo sem confundir taxa do loop com FPS renderizados.


## Resultado de hardware — PS4 Fat (capturas fornecidas pelo testador)

- GD Probe 0.2 inicia e renderiza tela texto.
- CPU checksum mostrado: `ba34abe5f2510398`, correspondente à rotina determinística de verificação.
- Testes de memória de 1 MiB e 4 MiB exibem `PASS`.
- Taxa de loop exibida ~38,5/s; não representa FPS do emulador nem FPS da GPU.
- Botão A entrou no modo framebuffer: quatro retângulos, barra e quadrado renderizados. Posições do quadrado mudaram entre duas capturas, compatíveis com animação funcional.
- Botão B voltou à tela texto. Eventos de controles e velocidade foram incrementados.
- **Problema de cores ainda aberto**: laranja definido como `RGBA8_MAXALPHA(255,107,0)` aparece azulado; azul `RGBA8_MAXALPHA(36,154,245)` aparece amarelado nas fotografias. Investigar ordem dos canais no caminho RGBA8888 (possível troca R/B); confirmar com padrão de cores calibrado e screenshots sem correção automática antes de alterar o renderizador ou aplicar swizzle permanente.
- A ausência de travamento aparente nas fotos não comprova encerramento: recuperar `boot.log` e conferir `the game exited` e ausência de faults.

## Próxima investigação

1. Registrar o log completo do GD36 após gráficos e retorno ao texto, com fase/encerramento, `status:` e eventuais falhas.
2. Criar probe de calibração com amostras R/G/B/W, verificando se troca R/B é consistente em áreas preenchidas e framebuffer.
3. Manter GD35 e GD36 como referências de estabilidade. Não inferir compatibilidade com jogos comerciais.

## Validacao do boot.log — GD36 no PS4 Fat

Trecho enviado pelo testador confirma:

- Build `gd-test36-20261010T024356Z-0498b1e860c4`; `gd-probe.nro` iniciado aos 96.999 s, fastmem OFF.
- Watchdog relatou `no new frame for 15 s during running` aos 98.252 s, mas novos quadros apareceram e a execução prosseguiu; investigar fase/instrumentação antes de assumir hang real.
- Status aos 108/118/128 s: 16.5, 35.6, 38.1 FPS; frame 30.2, 26.3, 26.3 ms.
- Status aos 138/148/158/168 s: 29.1, 19.2, 18.2, 19.5 FPS; frame 49.4, 54.7, 54.9, 48.0 ms.
- A queda coincide aproximadamente com testes gráficos descritos, mas `boot.log` não identifica o instante exato da troca entre console e framebuffer. **Não concluir causalidade exclusiva ou comparar cargas distintas como benchmark.**
- Memória guest+tabelas: 68-70 MiB; maior bloco livre direto 1978-1979 MiB. Lazy peak final: 69 MiB (métrica distinta do total de RAM).
- `the game exited` aos 177.479 s e `finished: close the app with the PS button` aos 177.485 s. Encerramento pelo guest confirmado.

Recomendacao GD37: medir as transições de modo, isolar custo de clear/full-frame CPU vs apresentação, testar formatos/canais de cor em imagens de referência e usar versões com área suja para comparar. Preservar fastmem OFF.
