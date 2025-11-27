import * as esbuild from 'esbuild';
import { chmod } from 'fs/promises';

await esbuild.build({
  entryPoints: ['source/cli.tsx'],
  bundle: true,
  platform: 'node',
  format: 'esm',
  target: 'esnext',
  outfile: 'dist/cli.js',
  banner: {
    js: `#!/usr/bin/env node
import{createRequire}from'module';const require=createRequire(import.meta.url);`
  }
});

await chmod('dist/cli.js', 0o755);
console.log('Build complete');
