// The GraphQLite docs (GQLITE-T-0376): Astro Starlight with the Colliery theme
// starlight-aurora, in the four Diátaxis sections.
import { defineConfig } from 'astro/config';
import starlight from '@astrojs/starlight';
import mermaid from 'astro-mermaid';
import starlightAurora from 'starlight-aurora';
import mdbookRedirects, { MDBOOK_EXTRA } from 'starlight-aurora/mdbook-redirects';
import starlightVersions from 'starlight-versions';

// The Pages workflow sets DOCS_SITE and DOCS_BASE. On a pull request and
// locally they are empty, and the site is at /.
const site = process.env.DOCS_SITE || undefined;
const base = process.env.DOCS_BASE || '/';

const PROJECT = 'GraphQLite';
const REPO = 'https://github.com/colliery-io/graphqlite';

// The sidebar is written out, not generated: it keeps the order and the short
// labels of the old SUMMARY.md. Add a new page here.
const page = (slug, label) => ({ slug, label });

const sidebar = [
	{ label: 'Introduction', link: '/' },
	{
		label: 'Tutorials',
		items: [
			page('tutorials/getting-started', 'Getting Started (Python)'),
			page('tutorials/sql-getting-started', 'Getting Started (SQL)'),
			page('tutorials/sql-patterns', 'Query Patterns (SQL)'),
			page('tutorials/knowledge-graph', 'Building a Knowledge Graph'),
			page('tutorials/graph-analytics', 'Graph Analytics'),
			page('tutorials/sql-algorithms', 'Graph Algorithms (SQL)'),
			page('tutorials/graphrag', 'Building a GraphRAG System'),
		],
	},
	{
		label: 'How-to guides',
		items: [
			page('how-to/installation', 'Installation'),
			page('how-to/multi-graph', 'Working with Multiple Graphs'),
			page('how-to/cli', 'Use the gqlite CLI'),
			page('how-to/graph-algorithms', 'Use Graph Algorithms'),
			page('how-to/special-characters', 'Handle Special Characters'),
			page('how-to/other-extensions', 'Use with Other Extensions'),
			page('how-to/parameterized-queries', 'Parameterized Queries'),
			page('how-to/bulk-import', 'Bulk Importing Data'),
			page('how-to/building-from-source', 'Building from Source'),
		],
	},
	{
		label: 'Reference',
		items: [
			{
				label: 'Cypher Support',
				items: [
					page('reference/cypher', 'Cypher Support'),
					page('reference/cypher-clauses', 'Clauses'),
					page('reference/cypher-functions', 'Functions'),
					page('reference/cypher-operators', 'Operators'),
				],
			},
			page('reference/algorithms', 'Graph Algorithms'),
			page('reference/python-api', 'Python API'),
			page('reference/rust-api', 'Rust API'),
			page('reference/sql-interface', 'SQL Interface'),
			page('reference/database-schema', 'Database Schema'),
			page('reference/earlier-versions', 'Earlier versions'),
		],
	},
	{
		label: 'Explanation',
		items: [
			page('explanation/architecture', 'Architecture'),
			page('explanation/storage-model', 'Storage Model'),
			page('explanation/query-dispatch', 'Query Dispatch'),
			page('explanation/performance', 'Performance'),
		],
	},
];

// The slug of each page in the sidebar.
const slugs = (items) => items.flatMap((i) => (i.items ? slugs(i.items) : i.slug ? [i.slug] : []));

// The mdBook site had the current docs at /latest/<page>.html. Those addresses
// go to the pages at the root. The targets are relative, for any base path.
const latest = {
	'latest/index.html': '../',
	'latest/introduction.html': '../',
	'latest/print.html': '../',
};
for (const slug of slugs(sidebar)) {
	latest[`latest/${slug}.html`] = `${'../'.repeat(slug.split('/').length)}${slug}/`;
}

export default defineConfig({
	site,
	base,
	// Mermaid is one large chunk, loaded only on a page with a diagram.
	vite: { build: { chunkSizeWarningLimit: 4096 } },
	integrations: [
		// Before Starlight. Light diagrams in the light theme; dark diagrams in
		// the dark theme and in hyper.
		mermaid({ theme: 'dark', autoTheme: true }),
		// The old mdBook addresses (how-to/x.html, latest/how-to/x.html) go to
		// the new pages.
		mdbookRedirects({ extra: { ...MDBOOK_EXTRA, ...latest } }),
		starlight({
			title: PROJECT,
			description: 'GraphQLite: a SQLite extension for graph queries in Cypher.',
			social: [{ icon: 'github', label: 'GitHub', href: REPO }],
			editLink: { baseUrl: `${REPO}/edit/main/docs/` },
			plugins: [
				starlightAurora({ hyper: true }),
				// Latest is the docs of main. The docs of each earlier release stay
				// at /<version>/. At a release, add its slug at the start of
				// `versions`, build, and commit the new files.
				starlightVersions({
					current: { label: 'Latest' },
					versions: [{ slug: '0.10.0', label: 'v0.10.0' }, { slug: '0.9.4', label: 'v0.9.4' }, { slug: '0.9.3', label: 'v0.9.3' }, { slug: '0.9.2', label: 'v0.9.2' }],
				}),
			],
			sidebar,
		}),
	],
});
