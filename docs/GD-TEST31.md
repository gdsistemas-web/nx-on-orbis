# GD Test 31 — Primeira etapa (em desenvolvimento)

## Teste real em PS4 Fat

| Build | Configuracao | Homebrew Menu |
| --- | --- | --- |
| Test 24 original | Sem fastmem experimental | Aproximadamente 60 FPS |
| GD Test 30 | fastmem=on | Aproximadamente 23 FPS |
| GD Test 30 | fastmem=off | Aproximadamente 60 FPS |

Na GD Test 30, o self-test da fastmem passou, mas o runtime registrou 0 paginas mapeadas, 2.648 redirecionamentos ao slow path e 605 entradas unaliasable. Nao sabemos ainda a causa raiz do mapeamento ou como isso afetara jogos.

## Mudancas iniciais nesta branch

- Fastmem desativada por padrao no PS4, apos ApplyPs4Settings; `fastmem=on` em `/data/edenps4/settings.txt` ainda permite testes.
- Se nao houver ROMs, o seletor de apps e apresentado com o Homebrew Menu integrado.
- Interface nativa GD Edition no seletor: tema escuro, cor de destaque laranja, lista e painel informativo, sem adicionar dependencias nem buffers.
- Todo o fluxo de video anterior ao Eden permanece inalterado.

## Validacao pendente

Esta etapa foi alterada remotamente no GitHub, mas nao foi compilada nem testada no PS4. Conferir com OpenOrbis e Eden antes de publicar um PKG. O proximo passo e implementar configuracoes/diagnosticos nativos interativos, examinar falhas de mapeamento e repetir testes A/B.

**Nao distribuir como release estavel.** Nao inclui firmware, keys ou jogos.
