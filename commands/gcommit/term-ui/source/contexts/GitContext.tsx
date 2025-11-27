import React, { createContext, useContext, useState, useEffect, useCallback, ReactNode } from 'react';
import simpleGit, { SimpleGit } from 'simple-git';

interface GitState {
  git: SimpleGit;
  originalBranch: string;
  stagingBranch: string | null;
  stashCreated: boolean;
  stagedDiff: string;
}

interface GitContextValue extends GitState {
  stashChanges: () => Promise<boolean>;
  popStash: () => Promise<void>;
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
  });

  // Get current branch on mount
  useEffect(() => {
    state.git.branch().then(b => {
      setState(s => ({ ...s, originalBranch: b.current }));
    });
  }, []);

  const stashChanges = useCallback(async (): Promise<boolean> => {
    const status = await state.git.status();

    // Check if there are any changes to stash (modified, not staged)
    const hasChanges = status.modified.length > 0 ||
                       status.not_added.length > 0 ||
                       status.deleted.length > 0;

    if (hasChanges) {
      await state.git.stash(['push', '-u', '-m', 'gcommit-auto-stash']);
      setState(s => ({ ...s, stashCreated: true }));
      return true;
    }
    return false;
  }, [state.git]);

  const popStash = useCallback(async () => {
    if (state.stashCreated) {
      try {
        await state.git.stash(['pop']);
        setState(s => ({ ...s, stashCreated: false }));
      } catch (err) {
        // Stash may have already been popped or doesn't exist
        console.error('Failed to pop stash:', err);
      }
    }
  }, [state.git, state.stashCreated]);

  const createStagingBranch = useCallback(async (): Promise<string> => {
    const branchName = `gcommit-staging-${Date.now()}`;
    await state.git.checkoutLocalBranch(branchName);
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
    await state.git.raw(['apply', '--unidiff-zero', patchPath]);
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
    // Pop stash if we created one
    if (state.stashCreated) {
      try {
        await state.git.stash(['pop']);
      } catch (err) {
        // Stash may not exist
      }
    }
  }, [state.git, state.stagingBranch, state.originalBranch, state.stashCreated]);

  const value: GitContextValue = {
    ...state,
    stashChanges,
    popStash,
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
