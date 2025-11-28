import React, { useState, useEffect, useCallback } from 'react';
import { Box, Text, useApp, useInput } from 'ink';
import { Spinner } from '@inkjs/ui';
import { fileURLToPath } from 'url';
import { dirname, join } from 'path';
import { GitProvider, useGit } from './contexts/GitContext.js';
import FileTree from './components/FileTree.js';
import DiffViewer from './components/DiffViewer.js';
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

  // Visualization state
  const [viewMode, setViewMode] = useState<'scatter' | 'diff'>('diff');
  const [selectedCluster, setSelectedCluster] = useState(0);
  const [focusPanel, setFocusPanel] = useState<'tree' | 'diff'>('tree');
  const [selectedFilePath, setSelectedFilePath] = useState<string>('');
  const [commitShas, setCommitShas] = useState<string[]>([]);
  const [fileDiffs, setFileDiffs] = useState<Map<string, Map<string, string>>>(new Map());

  // Callback for FileTree selection
  const handleFileSelect = useCallback((_index: number, filepath: string) => {
    setSelectedFilePath(filepath);
  }, []);

  // Global cleanup on process exit/interrupt
  useEffect(() => {
    const cleanup = async () => {
      try {
        await git.cleanup();
        if (phase !== 'error') {
          const fs = await import('fs/promises');
          await fs.rm('/tmp/patches', { recursive: true, force: true });
        }
      } catch {
        // Ignore cleanup errors
      }
    };

    const handleExit = () => { cleanup(); };
    const handleSignal = () => { cleanup().then(() => process.exit(1)); };

    process.on('exit', handleExit);
    process.on('SIGINT', handleSignal);
    process.on('SIGTERM', handleSignal);

    return () => {
      process.off('exit', handleExit);
      process.off('SIGINT', handleSignal);
      process.off('SIGTERM', handleSignal);
    };
  }, [git]);

  // Phase: Stashing
  useEffect(() => {
    if (phase !== 'stashing') return;

    const run = async () => {
      try {
        // Clean up any existing patches from previous runs
        setStatusMessage('Cleaning up previous patches...');
        const fs = await import('fs/promises');
        await fs.rm('/tmp/patches', { recursive: true, force: true });
        
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

        // Clear any old patches from previous runs
        const fs = await import('fs/promises');
        await fs.rm('/tmp/patches', { recursive: true, force: true });

        // Dynamic import to avoid loading execa at top level
        const { execa } = await import('execa');

        // Use the diff captured before stashing
        const diff = git.stagedDiff;

        // Spawn C++ binary (find it relative to this script's location)
        const scriptDir = dirname(fileURLToPath(import.meta.url));
        const binaryPath = join(scriptDir, 'git_gcommit.o');
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
        const shas: string[] = [];

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

          // Capture the commit SHA
          const sha = await git.git.revparse(['HEAD']);
          shas.push(sha);
        }

        setCommitMessages(messages);
        setCommitShas(shas);
        setPhase('visualization');
      } catch (err: any) {
        setError(err.message);
        setPhase('error');
      }
    };
    run();
  }, [phase, processingResult]);

  // Load per-file diffs when entering visualization phase
  useEffect(() => {
    if (phase !== 'visualization' || commitShas.length === 0) return;

    const loadDiffs = async () => {
      const allDiffs = new Map<string, Map<string, string>>();

      for (const sha of commitShas) {
        const perFileDiffs = new Map<string, string>();

        // Get list of changed files for this commit
        const filesOutput = await git.git.diff([`${sha}^`, sha, '--name-only']);
        const fileList = filesOutput.trim().split('\n').filter(f => f);

        // Get diff for each file
        for (const file of fileList) {
          const diff = await git.git.diff([`${sha}^`, sha, '--', file]);
          perFileDiffs.set(file, diff);
        }

        allDiffs.set(sha, perFileDiffs);
      }

      setFileDiffs(allDiffs);
    };

    loadDiffs();
  }, [phase, commitShas]);

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
        // Non-fatal - stash might not exist, but print the error
        console.error('Failed to pop stash:', err.message);
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
      } catch (err: any) {
        // Print cleanup errors for debugging
        console.error('Cleanup error during cancellation:', err.message);
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
        // Don't delete patches on error - keep for debugging
        // const fs = await import('fs/promises');
        // await fs.rm('/tmp/patches', { recursive: true, force: true });
      } catch (err) {
        // Ignore cleanup errors
      }
      exit();
    };
    run();
  }, [phase]);

  // Phase: Done
  useEffect(() => {
    if (phase !== 'done') return;

    // Clean up temp files and exit
    const run = async () => {
      try {
        const fs = await import('fs/promises');
        await fs.rm('/tmp/patches', { recursive: true, force: true });
      } catch (err) {
        // Ignore
      }
      exit();
    };
    run();
  }, [phase]);

  // Keyboard input for visualization phase
  useInput((input, key) => {
    if (phase !== 'visualization') return;

    const numClusters = commitShas.length;

    // View toggle
    if (input === 'v') {
      setViewMode(v => (v === 'scatter' ? 'diff' : 'scatter'));
    }

    // Panel toggle (diff view only)
    if (viewMode === 'diff' && key.tab) {
      setFocusPanel(p => (p === 'tree' ? 'diff' : 'tree'));
    }

    // Commit navigation with shift key (works from any panel)
    if (key.shift && (input === 'h' || input === 'H' || key.leftArrow)) {
      setSelectedCluster(c => Math.max(0, c - 1));
      setSelectedFilePath('');
    } else if (key.shift && (input === 'l' || input === 'L' || key.rightArrow)) {
      setSelectedCluster(c => Math.min(numClusters - 1, c + 1));
      setSelectedFilePath('');
    }

    // Apply/Cancel (always available)
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

  if (phase === 'visualization' && commitShas.length > 0) {
    const currentSha = commitShas[selectedCluster];
    const currentFiles = fileDiffs.get(currentSha || '');
    const fileList = currentFiles ? Array.from(currentFiles.keys()) : [];
    const currentDiff = currentFiles?.get(selectedFilePath) || '';
    const commitMessage = processingResult?.commits[selectedCluster]?.message || '';

    // Scatter view
    if (viewMode === 'scatter' && processingResult?.visualization) {
      return (
        <Box flexDirection="column">
          <ScatterPlot points={processingResult.visualization.points} />
          <ClusterLegend clusters={processingResult.visualization.clusters} />
          <Box marginTop={1}>
            <Text dimColor>h/l: commits · </Text>
            <Text color="yellow" bold>v</Text>
            <Text dimColor>: diff view · </Text>
            <Text color="green" bold>a</Text>
            <Text dimColor>: apply · </Text>
            <Text color="red" bold>q</Text>
            <Text dimColor>: quit</Text>
          </Box>
        </Box>
      );
    }

    // Diff view
    return (
      <Box flexDirection="column">
        {/* Header */}
        <Box marginBottom={1} borderStyle="single" paddingX={1}>
          <Text bold>
            Commit {selectedCluster + 1}/{commitShas.length}: {commitMessage.split('\n')[0]}
          </Text>
        </Box>

        {/* Two-panel layout */}
        <Box>
          <FileTree
            files={fileList}
            focused={focusPanel === 'tree'}
            onFileSelect={handleFileSelect}
          />
          <DiffViewer
            content={currentDiff}
            filepath={selectedFilePath}
            focused={focusPanel === 'diff'}
          />
        </Box>

        {/* Controls */}
        <Box marginTop={1}>
          <Text dimColor>
            TAB: panels · {focusPanel === 'tree' ? 'j/k: files' : 'j/k/h/l: scroll'} · shift+h/l: commits · </Text>
          <Text color="yellow" bold>v</Text>
          <Text dimColor>: scatter · </Text>
          <Text color="green" bold>a</Text>
          <Text dimColor>: apply · </Text>
          <Text color="red" bold>q</Text>
          <Text dimColor>: quit</Text>
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
