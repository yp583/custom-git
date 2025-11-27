import React from 'react';
import {Box, useApp, useInput} from 'ink';
import ScatterPlot from './components/ScatterPlot.js';
import ClusterLegend from './components/ClusterLegend.js';
import StatusBar from './components/StatusBar.js';

type Point = {
	id: number;
	x: number;
	y: number;
	cluster_id: number;
	filepath: string;
	preview: string;
};

type Cluster = {
	id: number;
	message: string;
};

type Props = {
	points: Point[];
	clusters: Cluster[];
	onContinue: () => void;
	onCancel: () => void;
};

export default function App({points, clusters, onContinue, onCancel}: Props) {
	const {exit} = useApp();

	useInput((input, key) => {
		if (input === 'q') {
			onContinue();
			exit();
		} else if (input === 'c' || key.escape) {
			onCancel();
			exit();
		}
	});

	return (
		<Box flexDirection="column">
			<ScatterPlot points={points} />
			<ClusterLegend clusters={clusters} />
			<StatusBar />
		</Box>
	);
}
