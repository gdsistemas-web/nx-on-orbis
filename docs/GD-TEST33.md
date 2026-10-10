# GD Test 33 — Correcoes de interface e diagnosticos

## Base de hardware

As fotos dos testes de GD Test 32 no PS4 Fat mostram:
- Triangulo abre a tela nativa de diagnosticos.
- Quadrado abre as configuracoes; fastmem esta desativada.
- O log corrente exibe `gd-test32-20261010T015434Z-0498b1e860c4`.
- O menu tinha textos fixos `TEST 31`; a telemetria de FPS estava `Sem medicao` porque, no menu anterior ao boot de jogos, o `boot.log` atual ainda nao contem FPS.
- O hbmenu integrado mostra `launchInit() failed` por nao ter hbloader; isso nao e evidencia de crash do emulador, nem validacao de jogos.

## Mudancas

- `dev/gd-test33` gera build ID `gd-test33-...` apos reconfigurar CMake.
- Package separado com Title ID `EDPS00033`, para nao substituir Test 31 nem Test 32.
- Cabecalho e painel lateral identificam corretamente GD Test 33.
- A tela de diagnosticos le `/data/edenps4/boot.old.log`, que o proprio `Ps4::OpenBootLog()` preserva antes de iniciar um novo `boot.log`. Dados e versao sao **da sessao anterior**; nao existe telemetria ao vivo nem escrita adicional no logger.
- Limites de leitura: 20.000 linhas; ignora linhas maiores que 1.024 caracteres; recorta strings ao desenhar.
- Fastmem segue OFF por padrao, com opcao de ativar em configuracoes para experimentos.

## Build no Linux Mint

```bash
cd ~/nx-on-orbis
git fetch origin
git switch -c dev/gd-test33 --track origin/dev/gd-test33
source tools/env-build.sh
unset OO_PS4_TOOLCHAIN
bash configure-eden.sh
cmake --build build-eden --target eden-ps4 -j4
grep -aom3 'gd-test33\|GD TEST 33' build-eden/bin/eden-ps4
bash package-eden.sh
ls -lh out-eden/*EDPS00033*.pkg
```

Observacao: e necessario passar por `bash configure-eden.sh`, mesmo em builds incrementais, porque o identificador vem do arquivo CMake `frontend/frontend.cmake`.

## Plano de teste

1. Instalar Test 33 lado a lado pelo GoldHEN, sem remover 31/32.
2. Abrir Triangulo e conferir que cabeçalho e painel lateral mostram TEST 33; o campo BUILD aponta para a sessao anterior.
3. Confirmar FPS e fastmem historicos, quando houver `boot.old.log` valido.
4. Executar Homebrew Menu para testar a performance; iniciar o app novamente e conferir os valores no menu.
5. Recuperar `/data/edenps4/boot.log` e verificar identificador e status em hardware.

**Estado: somente codigo no GitHub; build, console e FPS ainda nao testados na Test 33.**
