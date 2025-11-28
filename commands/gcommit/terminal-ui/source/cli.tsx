import React from 'react';
import { render } from 'ink';
import meow from 'meow';
import { simpleGit } from 'simple-git';
import App from './app.js';

const cli = meow(`
  Usage
    $ git gcommit [options]

  Options
    -d, --threshold  Clustering distance threshold (default: 0.5)
    -v, --verbose    Show verbose output from C++ binary
    -h, --help       Show this help message

  Examples
    $ git gcommit
    $ git gcommit -d 0.3
    $ git gcommit --threshold 0.7 --verbose
`, {
  importMeta: import.meta,
  flags: {
    threshold: {
      type: 'number',
      shortFlag: 'd',
      default: 0.5,
    },
    verbose: {
      type: 'boolean',
      shortFlag: 'v',
      default: false,
    },
  },
});

async function main() {
  const git = simpleGit();

  // Validation: Check if in a git repository
  const isRepo = await git.checkIsRepo();
  if (!isRepo) {
    console.error('Error: Not in a git repository');
    process.exit(1);
  }

  // Validation: Check for staged changes
  const staged = await git.diff(['--cached', '--name-only']);
  if (!staged.trim()) {
    console.error('Error: No staged changes. Use `git add` to stage files.');
    process.exit(1);
  }

  // Validation: Check for OPENAI_API_KEY
  if (!process.env['OPENAI_API_KEY']) {
    console.error('Error: OPENAI_API_KEY environment variable not set');
    process.exit(1);
  }

  // Launch Ink app
  const { waitUntilExit } = render(
    <App
      threshold={cli.flags.threshold}
      verbose={cli.flags.verbose}
    />
  );

  await waitUntilExit();
}

main().catch(err => {
  console.error('Error:', err.message);
  process.exit(1);
});
