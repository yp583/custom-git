import React, { useState, useEffect } from 'react';
import { Box, Text, useApp, useInput } from 'ink';
import { Spinner } from '@inkjs/ui';
import { GitProvider, useGit } from './contexts/GitContext.js';
import ScatterPlot from './components/ScatterPlot.js';
import ClusterLegend from './components/ClusterLegend.js';
import type { Phase, ProcessingResult } from './types.js';

type Props = {
  threshold: number;
  verbose: boolean;
};

function AppContent({ threshold, verbose }: Props) {
  const { exit } = useApp();
  const git = useGit();

  const [phase, setPhase] = useState<Phase>('stashing');
  const [processingResult, setProcessingResult] = useState<ProcessingResult | null>(null);
  const [commitMessages, setCommitMessages] = useState<string[]>([]);
  const [error, setError] = useState<string | null>(null);
  const [statusMessage, setStatusMessage] = useState('Initializing...');
  const [stderr, setStderr] = useState<string>('');

  // Phase: Stashing
  useEffect(() => {
    if (phase !== 'stashing') return;

    const run = async () => {
      try {
        setStatusMessage('Stashing working tree changes...');
        await git.stashChanges();
        setPhase('processing');
      } catch (err: any) {
        setError(err.message);
        setPhase('error');
      }
    };
    run();
  }, [phase]);

  // Phase: Processing (spawn C++ binary)
  useEffect(() => {
    if (phase !== 'processing') return;

    const run = async () => {
      try {
        setStatusMessage('Analyzing changes with AI...');

        // Dynamic import to avoid loading execa at top level
        const { execa } = await import('execa');
        const { simpleGit } = await import('simple-git');
        const localGit = simpleGit();

        // Get staged diff
        const diff = await localGit.diff(['--cached']);

        // Spawn C++ binary
        const binaryPath = process.env['HOME'] + '/bin/git_gcommit.o';
        const args = ['-d', String(threshold), '-i'];
        if (verbose) args.push('-v');

        const result = await execa(binaryPath, args, {
          input: diff,
          encoding: 'utf8',
        });

        // Store stderr for verbose display
        if (result.stderr) {
          setStderr(result.stderr);
        }

        // Parse JSON from stdout
        const data: ProcessingResult = JSON.parse(result.stdout);
        setProcessingResult(data);
        setPhase('applying');
      } catch (err: any) {
        setError(err.message);
        setPhase('error');
      }
    };
    run();
  }, [phase, threshold, verbose]);

  // Phase: Applying (create staging branch, apply patches, commit)
  useEffect(() => {
    if (phase !== 'applying' || !processingResult) return;

    const run = async () => {
      try {
        setStatusMessage('Creating staging branch...');
        await git.createStagingBranch();

        const messages: string[] = [];

        for (let i = 0; i < processingResult.commits.length; i++) {
          const commit = processingResult.commits[i]!;
          setStatusMessage(`Applying cluster ${i + 1}/${processingResult.commits.length}...`);

          // Apply each patch file
          for (const patchFile of commit.patch_files) {
            await git.applyPatch(patchFile);
          }

          // Stage and commit
          await git.stageAll();
          await git.commit(commit.message);
          messages.push(commit.message);
        }

        setCommitMessages(messages);
        setPhase('visualization');
      } catch (err: any) {
        setError(err.message);
        setPhase('error');
      }
    };
    run();
  }, [phase, processingResult]);

  // Phase: Merging
  useEffect(() => {
    if (phase !== 'merging') return;

    const run = async () => {
      try {
        setStatusMessage('Merging to original branch...');
        await git.mergeStagingBranch();
        setPhase('restoring');
      } catch (err: any) {
        setError(err.message);
        setPhase('error');
      }
    };
    run();
  }, [phase]);

  // Phase: Restoring stash
  useEffect(() => {
    if (phase !== 'restoring') return;

    const run = async () => {
      try {
        setStatusMessage('Restoring working tree...');
        await git.popStash();
        setPhase('done');
      } catch (err: any) {
        // Non-fatal - stash might not exist
        setPhase('done');
      }
    };
    run();
  }, [phase]);

  // Phase: Cancelled
  useEffect(() => {
    if (phase !== 'cancelled') return;

    const run = async () => {
      try {
        setStatusMessage('Cancelling...');
        await git.deleteStagingBranch();
        await git.popStash();
        // Clean up temp files
        const fs = await import('fs/promises');
        await fs.rm('/tmp/patches', { recursive: true, force: true });
      } catch (err) {
        // Ignore cleanup errors
      }
      exit();
    };
    run();
  }, [phase]);

  // Phase: Error
  useEffect(() => {
    if (phase !== 'error') return;

    const run = async () => {
      try {
        await git.cleanup();
      } catch (err) {
        // Ignore cleanup errors
      }
    };
    run();
  }, [phase]);

  // Phase: Done
  useEffect(() => {
    if (phase !== 'done') return;

    // Clean up temp files
    const run = async () => {
      try {
        const fs = await import('fs/promises');
        await fs.rm('/tmp/patches', { recursive: true, force: true });
      } catch (err) {
        // Ignore
      }
    };
    run();
  }, [phase]);

  // Keyboard input for visualization phase
  useInput((input, key) => {
    if (phase !== 'visualization') return;

    if (input === 'a') {
      setPhase('merging');
    } else if (input === 'q' || input === 'c' || key.escape) {
      setPhase('cancelled');
    }
  });

  // Render based on phase
  if (phase === 'stashing' || phase === 'processing' || phase === 'applying') {
    return (
      <Box flexDirection="column">
        <Spinner label={statusMessage} />
        {verbose && stderr && (
          <Box marginTop={1}>
            <Text dimColor>{stderr}</Text>
          </Box>
        )}
      </Box>
    );
  }

  if (phase === 'visualization' && processingResult) {
    return (
      <Box flexDirection="column">
        <ScatterPlot points={processingResult.visualization.points} />
        <ClusterLegend clusters={processingResult.visualization.clusters} />
        <Box marginTop={1}>
          <Text dimColor>Press </Text>
          <Text color="green" bold>a</Text>
          <Text dimColor> to apply commits · </Text>
          <Text color="red" bold>q</Text>
          <Text dimColor> to quit</Text>
        </Box>
      </Box>
    );
  }

  if (phase === 'merging' || phase === 'restoring') {
    return (
      <Box>
        <Spinner label={statusMessage} />
      </Box>
    );
  }

  if (phase === 'done') {
    return (
      <Box flexDirection="column">
        <Text color="green">✓ Created {commitMessages.length} commit(s) on {git.originalBranch}</Text>
        {commitMessages.map((msg, i) => (
          <Text key={i} dimColor>  - {msg.split('\n')[0]}</Text>
        ))}
      </Box>
    );
  }

  if (phase === 'cancelled') {
    return (
      <Box>
        <Text dimColor>Cancelled. No changes made.</Text>
      </Box>
    );
  }

  if (phase === 'error') {
    return (
      <Box flexDirection="column">
        <Text color="red">Error: {error}</Text>
        <Text dimColor>Cleaned up staging branch and restored stash.</Text>
      </Box>
    );
  }

  return null;
}

export default function App(props: Props) {
  return (
    <GitProvider>
      <AppContent {...props} />
    </GitProvider>
  );
}
