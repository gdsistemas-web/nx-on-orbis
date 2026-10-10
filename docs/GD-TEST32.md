# GD Test 32 — Desenvolvimento e validação

Branch experimental independente, derivada de `dev/gd-test31`. A GD Test 31 foi testada no PS4 Fat com o seletor e a tela de configurações funcionando e Homebrew Menu a 59,9–60 FPS usando fastmem desligada. O `hbmenu.nro` integrado mostra `launchInit() failed` porque espera o hbloader; isso não é prova de compatibilidade com jogos.

## Implementado

- Identificador do binário novo: `gd-test32-<timestamp>-<revision>`, definido em `frontend/frontend.cmake`. **A reconfiguração CMake é obrigatória** para atualizar o cache existente.
- Title ID distinto `EDPS00032` em `package-eden.sh` para coexistir com a Test 31.
- Menu nativo: Triângulo abre os diagnósticos; Círculo retorna. Quadrado abre configurações como antes.
- Diagnósticos exibem a última identificação de build e os últimos registros de status e fastmem encontrados em `/data/edenps4/boot.log`.
- Leituras do log são **históricas**, não telemetria ao vivo. A leitura tem limite de 20.000 linhas e ignora linhas maiores que 1.024 caracteres.
- Fastmem continua desligada por padrão.

## Compilar no Linux Mint

```bash
cd ~/nx-on-orbis
git fetch origin
git switch --track origin/dev/gd-test32
source tools/env-build.sh
unset OO_PS4_TOOLCHAIN
bash configure-eden.sh
cmake --build build-eden --target eden-ps4 -j4
grep -aom2 'gd-test32' build-eden/bin/eden-ps4
bash package-eden.sh
ls -lh out-eden/*EDPS00032*.pkg
```

Após a instalação, confirmar no log que a versão e o título do pacote correspondem à Test 32. O pacote não foi compilado nem testado no PS4 durante esta alteração remota. Não declarar como estável antes de validar.

## Pendências

- Desenvolver NRO de diagnóstico autônomo que não dependa de hbloader, após definir o toolchain/SDK Switch apropriado.
- Investigar o mapeamento fastmem sem comprometer a configuração estável.
- Melhorar a tela de diagnósticos e testar os limites de renderização no hardware.
