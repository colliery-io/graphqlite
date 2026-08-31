// Flat ESLint config for the TypeScript bindings. typescript-eslint's
// recommended set on the .ts sources; generated/vendored dirs are ignored.
import tseslint from 'typescript-eslint';

export default [
  { ignores: ['dist/**', 'node_modules/**', 'npm/**'] },
  ...tseslint.configs.recommended,
];
