# NX on Orbis

[English](README.md) | **Português (Brasil)**

> **Esta é a branch de continuação `dev/fastmem-v1`.** Ela é baseada na `experimental-fastmem` (testes 25–29) e adiciona o **GD Test 30**, com diagnóstico de hotspots de fastmem e um fluxo reproduzível de build/empacotamento no Linux. O PKG do Test 30 já compila e empacota com sucesso no Linux; a validação no console ainda está pendente. Veja [docs/GD-TEST30.md](docs/GD-TEST30.md).

Um port experimental do emulador de Nintendo Switch **Eden** (um fork do yuzu) para um
**PS4 Pro desbloqueado** (toolchain OpenOrbis, driver Vulkan Mesa RADV para a GPU do PS4).

> **Status: experimento originalmente encerrado por falta de tempo e publicado para que outras pessoas pudessem continuar.** Ele inicia, roda jogos reais e Mario Kart 8 Deluxe chegou a corridas completas, mas em **13–16 fps**, com os canais vermelho e azul trocados em toda a imagem. Não é uma forma prática de jogar jogos de Switch em um PS4. Leia [docs/STATUS.md](docs/STATUS.md) para ver as metas, os resultados alcançados e o teto de desempenho estimado.

| | |
|---|---|
| Console | PS4 Pro, firmware 12.02, GoldHEN (único console no qual o upstream foi testado) |
| Base | Eden `5f142c79` (commit fixado pelo port de PS5 ProsperoEden) + patches locais em `patches/eden/` |
| Toolchain | OpenOrbis 0.5.4 + [orbis-sdk-v1](https://github.com/orbis-ports/orbis-porting-kit/releases/tag/orbis-sdk-v1) (orbis-compat, Mesa RADV para “Liverpool” GFX7), LLVM 18 e libc++ 18 compilada para PS4 |
| Release estável anterior | [v0.1.0](../../releases/tag/v0.1.0): `.pkg` + ELF exato para simbolização de crashes |
| Teste atual | [v0.2.0-test30](../../releases/tag/v0.2.0-test30): build e PKG Linux concluídos; validação no PS4 pendente |
| Licença | GPL-3.0-or-later (veja `LICENSE` e `NOTICE.md`) |

## Sem afiliação com o Eden. Desenvolvido com apoio de IA.

- Este projeto **não é afiliado, endossado ou suportado pelo projeto Eden** ou por seus desenvolvedores. **Não reporte problemas deste port ao Eden** (issues, Discord ou qualquer outro canal).
- O projeto foi desenvolvido com uso intenso de assistentes de IA, dirigido e testado em console pelo autor original e, nesta continuação, com novas alterações e diagnósticos. O projeto Eden proíbe contribuições geradas por IA; por isso este port permanece separado e nada dele deve ser enviado ao upstream.
- Nenhuma key, firmware, jogo comercial ou arquivo de sistema da Sony é incluído. Use somente dumps de seus próprios dispositivos e jogos.

## Objetivos e até onde chegou

O objetivo original era rodar Mario Kart 8 Deluxe em um PS4 Pro através do Eden: primeiro chegar a uma corrida, depois melhorar o desempenho e então corrigir as cores. A primeira parte foi alcançada; desempenho e cores ainda não foram resolvidos.

| Objetivo | Resultado |
|---|---|
| Eden rodar como app nativo de PS4 | Sim |
| Um jogo comercial iniciar | Sim: menus do MK8D em 40–60 fps; menu do Cuphead em ~30 fps |
| MK8D chegar a uma corrida | Sim: as corridas rodam e o áudio é limpo |
| Desempenho jogável | Não: 13–16 fps nas corridas (teto estimado neste console ~20–25 fps) |
| Cores corretas | Não: vermelho e azul ficam trocados em toda a imagem (3D, vídeos e UI) |

**Testado pelo upstream em:** um PS4 Pro com firmware 12.02 e GoldHEN; MK8D (jogo base), Cuphead (menu e início do jogo) e Homebrew Menu; modo portátil; sessões de cerca de 15 minutos. **29 builds de teste** foram executados no console em cinco dias. Todos estão documentados em [docs/STATUS.md](docs/STATUS.md#every-test-on-the-console).

## O que funciona (v0.1.0, medido no console)

- Inicializa com seletor próprio de jogos (Cima/Baixo + X) pela saída de vídeo do PS4; entrada pelo DualShock 4; áudio via `sceAudioOut` sem estalos.
- **Mario Kart 8 Deluxe**: tela inicial e menus em 40–60 fps; corridas carregam e rodam em **13–16 fps**, com travamentos momentâneos durante compilação de novos shaders.
- Cuphead chega ao menu em ~30 fps.
- O Homebrew Menu incluído (nx-hbmenu) roda sem keys.

## O que ainda não funciona

- **Cores**: vermelho e azul aparecem trocados em toda a imagem do jogo: modelos 3D, vídeos e interface. As causas já testadas relacionadas a sampling/formato foram descartadas; o caminho de apresentação passou a ser o principal suspeito. Veja `docs/STATUS.md`.
- **Desempenho**: a CPU emulada é o gargalo; a GPU não aparece como limitante principal. Veja `docs/STATUS.md` para o perfil e o teto estimado.
- **Memória**: uma corrida usa aproximadamente 4,5 GiB dos ~4,6 GiB de memória direta disponíveis ao processo. Travamentos ou crashes ao carregar corridas provavelmente estão relacionados à exaustão de memória.
- **GD Test 30**: ainda não foi validado em hardware. O objetivo é medir em quais páginas guest se concentram os redirects lentos do fastmem.

## GD Test 30

A continuação em `dev/fastmem-v1` adiciona:

- tabela fixa de hotspots de redirect no caminho de exceção;
- contagem por página guest de 4 KiB;
- trabalho limitado dentro do handler, sem alocação, log ou locks;
- relatório periódico das páginas com mais redirects;
- contadores de páginas rastreadas e colisões;
- identificação de causas de redirect;
- build ID `gd-test30`;
- fluxo completo de build Linux;
- empacotamento PKG no Linux.

O build Linux do Test 30 concluiu com:

```text
[1550/1550] Linking CXX executable bin/eden-ps4
```

Artefatos congelados para o primeiro teste em hardware:

```text
out-eden/IV0000-EDPS00001_00-EDENPS4000000000.pkg
SHA-256: 3437859813eca8aaeddfb2173f32c99e4d196a73f97c790ba49aff6b42bfc547

elf/eden-ps4-gd-test30-20261007-2012.elf
SHA-256: 87ba17b441cbf49af6506f8439ad31458e1c985b9a990ec15865d38e205ea3f9
```

Não use um ELF diferente para simbolizar crashes desse PKG.

## Documentação

- [docs/STATUS.md](docs/STATUS.md): objetivos, resultados teste a teste, profiling, teto de desempenho estimado e pontos de continuação.
- [docs/INSTALL.md](docs/INSTALL.md): instalação do release e organização dos seus próprios arquivos.
- [docs/BUILDING.md](docs/BUILDING.md): rebuild completo, incluindo o caminho Linux validado.
- [docs/GD-TEST30.md](docs/GD-TEST30.md): bring-up Linux do GD Test 30, correções, artefatos gerados e plano de validação no PS4.
- [docs/TECHNICAL.md](docs/TECHNICAL.md): descobertas de plataforma, memória, GPU, tratamento de faults e limites de espaço de endereçamento.
- [docs/dev-log-es.md](docs/dev-log-es.md): log completo do desenvolvimento original em espanhol.

## Branches

- `main`: v0.1.0, último build do upstream que chegou a corridas de forma confiável (internamente “test 24”).
- `experimental-fastmem`: testes 25–29. Fastmem ativo com view de 8 GiB, redirects do JIT para o slow path do Dynarmic, mapa de memória direta e experimentos de redução da arena de GPU. Não voltou a carregar corrida por falta de memória direta.
- `dev/fastmem-v1`: continuação GD Test 30. Adiciona diagnóstico de hotspots, bootstrap das dependências no Linux, build do Eden no Linux e geração do PKG. Build e empacotamento validados; teste no console pendente.

## Créditos

Desenvolvedores do Eden e yuzu (emulador), ProsperoEden (port de PS5 que serviu de referência e definiu o commit do Eden), projeto orbis-ports (orbis-compat e driver PS4 Mesa RADV), OpenOrbis (toolchain), switchbrew (nx-hbmenu) e fontes DejaVu.

Port original por Alejo ([@alechurri](https://github.com/alechurri)). Continuação GD Test 30 e suporte de build Linux no fork `gdsistemas-web/nx-on-orbis`.
