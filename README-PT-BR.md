# Stream SWBR — detalhes da edição em português

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

## Informações em português: automático

A revisão 4 consulta o serviço público [TMDB Addon / ElfHosted](https://tmdb.elfhosted.com/) em pt-BR. O usuário não precisa criar conta, hospedar servidor ou digitar token. O serviço recebe o IP e os identificadores/títulos consultados para responder às buscas. Nenhuma credencial pessoal é enviada a esse serviço.

Foram verificadas consultas reais sem autenticação para A Origem, Big Bang: A Teoria e a busca por A Origem. Títulos, sinopses e episódios são armazenados em cache local; as consultas em segundo plano são limitadas a no máximo 80 por minuto por console, além dos limites do provedor. Se o serviço falhar, há uma pausa antes de novas tentativas e a navegação/reprodução continuam disponíveis.

Nas primeiras aberturas, os nomes podem aparecer no idioma original enquanto os dados são carregados. Algumas obras ou episódios podem não ter tradução; nomes oficiais brasileiros podem ser iguais aos originais. Animes identificados apenas por Kitsu e artes das capas podem continuar no idioma original. A disponibilidade do serviço público é de responsabilidade de terceiros.

Se já havia token pessoal salvo, ele continua sendo enviado somente à API oficial do TMDB. Para mudar, use Configurações → Informações em português → Usar automático. Se existir uma linha `tmdb_token=` em `/switch/streamfin-addon.txt`, remova-a para ela não ser importada de novo na próxima abertura. O pacote não inclui tokens ou chaves.

## Ajustes visuais da revisão 4

- Títulos dos cartões em duas linhas, fonte 17 e margem de 10 abaixo da capa; cartões mais altos preservam o tamanho dos pôsteres. Nomes muito longos ainda podem ser abreviados.
- Estrela dourada com fundo escuro no lugar da bolinha da biblioteca.
- Categorias com conteúdo centralizado horizontal e verticalmente, fundo discreto, detalhes coloridos e o destaque de foco já existente.
- A correção de inicialização da revisão 3 está preservada: o espaçamento fica no conteúdo, nunca no HScrollingFrame.

## Ajustes da revisão 5

- O título agora usa um componente próprio com até duas linhas e recorte estrito nos limites do cartão. A quebra não depende da medição automática de altura da biblioteca, que era ignorada quando largura e altura estavam fixas.
- Ano menor e fundo sutil por cartão; capas, títulos e ano permanecem em uma única coluna por filme.
- Botão + abre Configurações na tela inicial; legenda de X, Y, − e + na tela inicial e nas configurações.
- Detalhes com coluna de informações rolável, título menor, metadados discretos, cabeçalho Sinopse e margens entre as seções.
- Tradução em lote dos catálogos públicos popular/em alta (até 20 nomes por consulta), associada por IMDb sem trocar os itens da biblioteca. Os nomes são guardados separadamente por 30 dias; detalhes e episódios mantêm o cache existente. Títulos fora desses lotes continuam sendo consultados individualmente em segundo plano.
- Testes verificam associação por links IMDb válidos, bloqueio de associação por domínio parecido, cache do lote, retorno imediato dos nomes já armazenados e preservação de IDs. O novo layout requer confirmação no Switch.

## Stream SWBR 1.0.0 — identidade e navegação

- Logo horizontal fornecida pelo usuário, editada com fundo transparente, sem slogan ou bandeira no R, com SWBR em degradê verde; símbolo também aplicado ao ícone do hbmenu.
- Header fixo: logo à esquerda, subtítulo “Sua biblioteca de filmes e séries no Nintendo Switch”, configurações e busca à direita. O header é preservado ao reorganizar categorias.
- Fileiras de catálogo mostram até 15 títulos e um cartão Ver todos. A grade aberta por Ver todos mantém a lista completa recebida do catálogo, sem o limite da prévia.
- Instruções dos controles aparecem somente nas configurações. O atalho + permanece disponível na tela inicial.
- Nome de exibição do NRO: Stream SWBR; autor da edição: Diguit; versão 1.0.0; créditos originais preservados. O arquivo continua StreamFin.nro e a pasta de dados continua config/StreamFin para compatibilidade com instalações existentes.

## Versão 1.1.0 — complementos salvos

Adição, seleção, renomeação, alteração de endereço e exclusão de até 20 complementos. Instalações novas não incluem endereço de vídeo; instalações antigas preservam o endereço atual. Apenas o selecionado é utilizado, e excluir o ativo não seleciona outro automaticamente. Arquivos de importação inalterados não restauram endereços excluídos nem sobrescrevem a seleção. O arquivo stremio_addon.json mantém compatibilidade com o campo url anterior e inclui a lista addons.

O fluxo inicial e o botão − abrem o gerenciador; pode-se voltar para navegar sem configurar vídeo. A versão reconhece finais manifest.json e manifest/json. O pacote usa switch/Stream-SWBR-1.1.0/Stream-SWBR-1.1.0.nro.

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

Testes C++ cobrem classificação das fontes, busca sem distinção de acentos, persistência das preferências, títulos/sinopses, episódios, busca em português, preservação de IDs, cache e falha de metadados. Os testes de rede usam respostas simuladas; as consultas reais ao serviço público foram verificadas no computador; a confirmação visual e de reprodução da revisão 4 depende do teste no console.

Dados: TMDB — https://www.themoviedb.org . This product uses the TMDB API but is not endorsed or certified by TMDB.

## Versão 1.1.1 — header compacto

Logo reduzida de 440×88 para 240×48; header de 152 para 96 de altura; subtítulo e botões menores. Dados existentes em config/StreamFin são preservados.
