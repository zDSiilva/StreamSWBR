# StreamFinEX em português

Edição baseada em Kirbosh/StreamFinEX, commit `4ea3cecb3afd91b3b7feff7595b4b74a6e883b4a`, com licença Apache 2.0 e créditos originais preservados.

## O que mudou

- Menus do StreamFinEX em português brasileiro; o player usa os recursos de português existentes, que incluem algumas expressões de Portugal.
- Botão Configurações no lugar do logotipo da tela inicial: categorias, complementos, servidor preferido, áudio, legendas e cache de traduções.
- Fileiras horizontais de capas preservadas, com títulos mais destacados e cartão Ver todos para abrir os títulos carregados de cada categoria em uma grade.
- Editor de categorias em cartões com ordem numerada, estado visível/oculto, salvar e restaurar padrão. O formato de fileiras da tela inicial é preservado.
- Seleção de vídeos em cartões com servidor, resolução e áudio, quando identificáveis nos dados recebidos. Malfoy é o preferido inicial; pode ser trocado ou desativado em Configurações.
- Filtros Todas as opções, Dublados/áudio duplo e Legendados. X mostra o texto original fornecido pelo complemento.
- Preferência de áudio em português, com alternativas em Configurações. Legendas em português quando disponíveis ou desativadas ao iniciar.
- Integração com TMDB em pt-BR para nomes, sinopses, gêneros, busca e informações de episódios. Os IDs IMDb, URLs de vídeo e histórico são preservados.

## Informações em português: configurar uma vez

A revisão visual 2 corrige o espaçamento dos acentos usando a fonte latina do Switch, separa títulos e descrições dos botões e destaca o filtro ativo com cor. A tela inicial ganhou atalhos horizontais de categorias. Em Configurações, a opção de informações em português agora abre um tutorial navegável com QR code para a página oficial do TMDB e o botão Inserir token.

O TMDB exige uma credencial própria. O pacote não inclui credenciais.

1. Na sua conta em https://www.themoviedb.org/settings/api , obtenha um **token de leitura da API** (API Read Access Token).
2. No aplicativo, entre em **Configurações → Informações em português · TMDB** e informe somente o token, sem escrever `Bearer`.
3. Reabra a tela inicial para carregar os títulos traduzidos. Em uma primeira abertura, as capas podem mostrar o título original enquanto a tradução é obtida em segundo plano.

Para evitar digitar um token longo no Switch, você também pode adicionar esta linha ao arquivo `/switch/streamfin-addon.txt` do cartão SD, mantendo o endereço do seu complemento nas outras linhas:

```text
tmdb_token=SEU_TOKEN_DE_LEITURA_DO_TMDB
```

O token fica no cartão SD, em `config/StreamFin/preferences-pt.json`, e é enviado somente à API do TMDB por HTTPS. Não publique esse arquivo nem envie seu token em capturas de tela. Se usar o arquivo de importação, edite/remova a linha nele para evitar que ele sobrescreva uma mudança feita na interface.

Nem todo título tem tradução cadastrada. Nomes oficiais brasileiros podem ser iguais aos originais. Quando não houver sinopse em português, o aplicativo avisa; não inventa uma tradução. Animes identificados apenas por Kitsu ainda podem conservar o nome original, pois esta integração usa o vínculo com IMDb. As artes das capas não são traduzidas por esta edição.

## Gerar o arquivo para Switch

O workflow `.github/workflows/build-switch.yaml` executa os testes e compila `StreamFin.nro`. Ele roda ao enviar mudanças para `main` ou uma branch `ptbr/**`, e também permite execução manual.

Na aba **Actions** do repositório, abra **Build Switch NRO PT-BR**. Quando a execução concluir com sucesso, baixe o artefato **StreamFin-PT-BR-NintendoSwitch** e extraia `StreamFin.nro`.

O workflow não publica uma release automaticamente. Uma cópia em branch separada não substitui a branch principal de outro projeto.

## Instalar e testar

1. Guarde no computador uma cópia do `.nro` que você usa hoje.
2. Com o aplicativo fechado, copie o novo `StreamFin.nro` para a pasta `/switch/` do cartão SD.
3. Confira Configurações, categorias, busca por nome brasileiro, detalhes e episódios.
4. Escolha um vídeo, alterne os filtros com Y e confira os detalhes com X. Verifique o destaque de Malfoy sem excluir as outras fontes.
5. Teste áudio, legendas, retorno à seleção de vídeos e retomada do histórico no console.

Um servidor marcado como preferido não garante disponibilidade, qualidade ou ausência de travamentos. As informações de resolução e áudio dependem do que o complemento informa.

## Verificações

Testes C++ cobrem classificação das fontes, busca sem distinção de acentos, persistência das preferências, títulos/sinopses, episódios, busca em português, preservação de IDs, cache e falha de metadados. Os testes de rede usam respostas simuladas; uma credencial real do TMDB e o console são necessários para validar o funcionamento completo.

Dados: TMDB — https://www.themoviedb.org . This product uses the TMDB API but is not endorsed or certified by TMDB.
