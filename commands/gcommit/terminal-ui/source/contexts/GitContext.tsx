import React, { createContext, useContext, useState, useEffect, useCallback, ReactNode } from 'react';
import simpleGit, { SimpleGit } from 'simple-git';

interface GitState {
  git: SimpleGit;
  originalBranch: string;
  stagingBranch: string | null;
  unstagedStashCreated: boolean;
  stagedStashCreated: boolean;
  stagedDiff: string;
}

interface GitContextValue extends GitState {
  stashChanges: () => Promise<boolean>;
  popUnstagedStash: () => Promise<void>;
  dropStagedStash: () => Promise<void>;
  createStagingBranch: () => Promise<string>;
  deleteStagingBranch: () => Promise<void>;
  mergeStagingBranch: () => Promise<void>;
  applyPatch: (patchContent: string) => Promise<void>;
  stageAll: () => Promise<void>;
  commit: (message: string) => Promise<void>;
  cleanup: () => Promise<void>;
}

const GitContext = createContext<GitContextValue | null>(null);

interface GitProviderProps {
  children: ReactNode;
}

export function GitProvider({ children }: GitProviderProps) {
  const [state, setState] = useState<GitState>({
    git: simpleGit(),
    originalBranch: '',
    stagingBranch: null,
    stashCreated: false,
    stagedDiff: '',
  });

  // Get current branch on mount
  useEffect(() => {
    state.git.branch().then(b => {
      setState(s => ({ ...s, originalBranch: b.current }));
    });
  }, []);

  const stashChanges = useCallback(async (): Promise<boolean> => {
    // Capture staged diff BEFORE any stashing
    const diff = await state.git.diff(['--cached']);
    setState(s => ({ ...s, stagedDiff: diff }));

    const status = await state.git.status();

    // Check for unstaged changes (modified working tree, untracked files)
    const hasUnstagedChanges = status.modified.length > 0 ||
                               status.not_added.length > 0 ||
                               status.deleted.length > 0;

    // Check for staged changes
    const hasStagedChanges = status.staged.length > 0 ||
                             status.renamed.length > 0 ||
                             status.created.length > 0;

    // Step 1: Stash unstaged changes first (keep staged in index)
    if (hasUnstagedChanges) {
      await state.git.stash(['push', '-k', '-u', '-m', 'gcommit-unstaged']);
      setState(s => ({ ...s, unstagedStashCreated: true }));
    }

    // Step 2: Stash staged changes (now working tree = index = staged state)
    if (hasStagedChanges) {
      await state.git.stash(['push', '-m', 'gcommit-staged']);
      setState(s => ({ ...s, stagedStashCreated: true }));
    }

    return hasUnstagedChanges || hasStagedChanges;
  }, [state.git]);

  const popUnstagedStash = useCallback(async () => {
    if (!state.unstagedStashCreated) return;

    try {
      await state.git.stash(['pop']);
    } catch (err) {
      console.error('Failed to pop unstaged stash:', err);
    }

    setState(s => ({ ...s, unstagedStashCreated: false }));
  }, [state.git, state.unstagedStashCreated]);

  const dropStagedStash = useCallback(async () => {
    if (!state.stagedStashCreated) return;

    try {
      await state.git.stash(['drop']);
    } catch (err) {
      console.error('Failed to drop staged stash:', err);
    }

    setState(s => ({ ...s, stagedStashCreated: false }));
  }, [state.git, state.stagedStashCreated]);

  const createStagingBranch = useCallback(async (): Promise<string> => {
    const branchName = `gcommit-staging-${Date.now()}`;
    await state.git.checkoutLocalBranch(branchName);
    // Reset staged changes so files are at their original locations
    // This allows patches (including renames) to be applied cleanly
    await state.git.reset(['HEAD']);
    setState(s => ({ ...s, stagingBranch: branchName }));
    return branchName;
  }, [state.git]);

  const deleteStagingBranch = useCallback(async () => {
    if (state.stagingBranch) {
      // First checkout back to original branch
      await state.git.checkout(state.originalBranch);
      // Then delete the staging branch
      await state.git.deleteLocalBranch(state.stagingBranch, true);
      setState(s => ({ ...s, stagingBranch: null }));
    }
  }, [state.git, state.stagingBranch, state.originalBranch]);

  const mergeStagingBranch = useCallback(async () => {
    if (state.stagingBranch) {
      // Checkout original branch
      await state.git.checkout(state.originalBranch);
      // Merge staging branch
      await state.git.merge([state.stagingBranch]);
      // Delete staging branch
      await state.git.deleteLocalBranch(state.stagingBranch, true);
      setState(s => ({ ...s, stagingBranch: null }));
    }
  }, [state.git, state.stagingBranch, state.originalBranch]);

  const applyPatch = useCallback(async (patchPath: string) => {
    try {
      await state.git.raw(['apply', '--unidiff-zero', patchPath]);
    } catch (err: any) {
      throw new Error(`Failed to apply patch ${patchPath}: ${err.message}`);
    }
  }, [state.git]);

  const stageAll = useCallback(async () => {
    await state.git.add('-A');
  }, [state.git]);

  const commit = useCallback(async (message: string) => {
    await state.git.commit(message);
  }, [state.git]);

  const cleanup = useCallback(async () => {
    // Delete staging branch if it exists
    if (state.stagingBranch) {
      try {
        await state.git.checkout(state.originalBranch);
        await state.git.deleteLocalBranch(state.stagingBranch, true);
      } catch (err) {
        // Branch may not exist
      }
    }

    // Clean untracked files that may have been created by failed patches
    // This prevents "would be overwritten" errors when popping stash
    try {
      await state.git.clean('f', ['-d']);
    } catch {
      // Ignore clean errors
    }

    // Pop staged stash first (it's stash@{0} - most recent)
    if (state.stagedStashCreated) {
      try {
        await state.git.stash(['pop', '--index']);
      } catch (err) {
        try {
          await state.git.stash(['pop']);
        } catch {
          // Stash may not exist
        }
      }
    }

    // Pop unstaged stash second (it's now stash@{0} after first pop)
    if (state.unstagedStashCreated) {
      try {
        await state.git.stash(['pop']);
      } catch {
        // Stash may not exist
      }
    }
  }, [state.git, state.stagingBranch, state.originalBranch, state.stagedStashCreated, state.unstagedStashCreated]);

  const value: GitContextValue = {
    ...state,
    stashChanges,
    popUnstagedStash,
    dropStagedStash,
    createStagingBranch,
    deleteStagingBranch,
    mergeStagingBranch,
    applyPatch,
    stageAll,
    commit,
    cleanup,
  };

  return (
    <GitContext.Provider value={value}>
      {children}
    </GitContext.Provider>
  );
}

export function useGit(): GitContextValue {
  const context = useContext(GitContext);
  if (!context) {
    throw new Error('useGit must be used within a GitProvider');
  }
  return context;
}
