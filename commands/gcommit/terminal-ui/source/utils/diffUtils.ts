import type { DiffLine } from '../types.js';

/**
 * Parse a unified diff with full context (-U999999).
 * Since the diff already shows the full file, we just parse the +/- lines directly.
 */
export function parseFullContextDiff(diff: string): DiffLine[] {
  const result: DiffLine[] = [];

  if (!diff.trim()) {
    return result;
  }

  const lines = diff.split('\n');
  let inHunk = false;

  for (const line of lines) {
    // Skip diff headers
    if (line.startsWith('diff --git') ||
        line.startsWith('index ') ||
        line.startsWith('--- ') ||
        line.startsWith('+++ ')) {
      continue;
    }

    // Hunk header - start processing content
    if (line.startsWith('@@')) {
      inHunk = true;
      continue;
    }

    if (!inHunk) continue;

    if (line.startsWith('+')) {
      result.push({ content: line.slice(1), type: 'addition' });
    } else if (line.startsWith('-')) {
      result.push({ content: line.slice(1), type: 'deletion' });
    } else if (line.startsWith(' ')) {
      result.push({ content: line.slice(1), type: 'context' });
    } else if (line === '') {
      // Empty line in diff (context)
      result.push({ content: '', type: 'context' });
    } else if (line.startsWith('\\')) {
      // "\ No newline at end of file" - skip
      continue;
    }
  }

  return result;
}
