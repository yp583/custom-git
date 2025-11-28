import React from 'react';
import {Text, Box} from 'ink';

// createRequire is injected by esbuild banner
declare const require: (id: string) => unknown;
const Canvas = require('drawille') as new (width: number, height: number) => {
	set: (x: number, y: number) => void;
	frame: () => string;
};

type Point = {
	id: number;
	x: number;
	y: number;
	cluster_id: number;
	filepath: string;
	preview: string;
};

type Props = {
	points: Point[];
	width?: number;
	height?: number;
};

const COLORS: Array<'red' | 'cyan' | 'blue' | 'green' | 'yellow' | 'magenta' | 'white' | 'gray'> = [
	'red',
	'cyan',
	'blue',
	'green',
	'yellow',
	'magenta',
	'white',
	'gray',
];

// Braille base character and dot positions
const BRAILLE_BASE = 0x2800;

export default function ScatterPlot({points, width = 120, height = 60}: Props) {
	if (points.length === 0) {
		return <Text>No points to display</Text>;
	}

	// Find bounds
	const xs = points.map(p => p.x);
	const ys = points.map(p => p.y);
	const minX = Math.min(...xs);
	const maxX = Math.max(...xs);
	const minY = Math.min(...ys);
	const maxY = Math.max(...ys);

	const rangeX = maxX - minX || 1;
	const rangeY = maxY - minY || 1;

	// Group points by cluster
	const clusterIds = [...new Set(points.map(p => p.cluster_id))].sort((a, b) => a - b);

	// Create a canvas per cluster
	type CanvasInstance = {set: (x: number, y: number) => void; frame: () => string};
	const canvases = new Map<number, CanvasInstance>();
	for (const cid of clusterIds) {
		canvases.set(cid, new Canvas(width, height));
	}

	// Plot points to their cluster's canvas - one dot per point
	for (const p of points) {
		const canvas = canvases.get(p.cluster_id);
		if (!canvas) continue;

		const px = Math.floor(((p.x - minX) / rangeX) * (width - 4)) + 2;
		const py = Math.floor(((p.y - minY) / rangeY) * (height - 4)) + 2;

		if (px >= 0 && px < width && py >= 0 && py < height) {
			canvas.set(px, py);
		}
	}

	// Get frames from each canvas
	const frames = new Map<number, string[]>();
	let maxRows = 0;
	let maxCols = 0;

	for (const [cid, canvas] of canvases) {
		const frame = canvas.frame();
		const rows = frame.split('\n');
		frames.set(cid, rows);
		maxRows = Math.max(maxRows, rows.length);
		for (const row of rows) {
			maxCols = Math.max(maxCols, [...row].length);
		}
	}

	// Merge frames with colors - for each character position, pick the cluster that has content
	const mergedRows: React.ReactNode[] = [];

	for (let rowIdx = 0; rowIdx < maxRows; rowIdx++) {
		const segments: React.ReactNode[] = [];
		let currentCluster = -1;
		let currentChars = '';

		for (let colIdx = 0; colIdx < maxCols; colIdx++) {
			// Find which cluster has a non-empty braille char at this position
			let foundCluster = -1;
			let foundChar = ' ';

			for (const cid of clusterIds) {
				const rows = frames.get(cid);
				if (!rows || rowIdx >= rows.length) continue;

				const row = rows[rowIdx]!;
				const chars = [...row];
				if (colIdx >= chars.length) continue;

				const char = chars[colIdx]!;
				// Check if it's a non-empty braille character
				if (char.charCodeAt(0) > BRAILLE_BASE) {
					foundCluster = cid;
					foundChar = char;
					break; // First cluster wins for this cell
				}
			}

			if (foundCluster === currentCluster) {
				currentChars += foundChar;
			} else {
				// Flush current segment
				if (currentChars) {
					if (currentCluster === -1) {
						segments.push(<Text key={`${rowIdx}-${colIdx}-e`}>{currentChars}</Text>);
					} else {
						segments.push(
							<Text key={`${rowIdx}-${colIdx}`} color={COLORS[currentCluster % COLORS.length]}>
								{currentChars}
							</Text>
						);
					}
				}
				currentCluster = foundCluster;
				currentChars = foundChar;
			}
		}

		// Flush final segment
		if (currentChars) {
			if (currentCluster === -1) {
				segments.push(<Text key={`${rowIdx}-final-e`}>{currentChars}</Text>);
			} else {
				segments.push(
					<Text key={`${rowIdx}-final`} color={COLORS[currentCluster % COLORS.length]}>
						{currentChars}
					</Text>
				);
			}
		}

		mergedRows.push(<Box key={rowIdx}>{segments}</Box>);
	}

	const clusterCount = clusterIds.length;

	return (
		<Box flexDirection="column">
			<Text bold>UMAP Embedding Visualization</Text>
			<Box flexDirection="column" borderStyle="single" borderColor="gray">
				{mergedRows}
			</Box>
			<Text dimColor>
				{points.length} chunks · {clusterCount} clusters
			</Text>
		</Box>
	);
}
