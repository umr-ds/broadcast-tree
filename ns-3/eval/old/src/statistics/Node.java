package statistics;

import statistics.Statistics.FRAME_TYPE;

public class Node
{
	private int x, y;
	private int depth;
	private int cycles;
	private String macAddr;
	private double txPower;
	private long finishTime;
	private boolean finished;
	private int missingPackets;
	
	private int[][] data;
	private int[][] packets;
	private double[][] energy;
	
	private Node parent;
	
	
	public Node()
	{
		this.depth = 0;
		this.data = new int[8][2];
		this.packets = new int[8][2];
		this.energy = new double[8][2];
	}
	
	
	/*
	 * Position
	 */
	public int getPosX()
	{
		return this.x;
	}
	
	public int getPosY()
	{
		return this.y;
	}
	
	public void setPosition(int x, int y)
	{
		this.x = x;
		this.y = y;
	}
	
	
	/*
	 * Tree depth
	 */
	public int getTreeDepth()
	{
		return this.depth;
	}
	
	public void setTreeDepth(int d)
	{
		this.depth = d;
	}
	
	
	/*
	 * MAC Address
	 */
	public String getMacAddress()
	{
		return this.macAddr;
	}
	
	public void setMacAddress(String addr)
	{
		this.macAddr = addr;
	}
	
	
	/*
	 * TX Power
	 */
	public double getTxPower()
	{
		return this.txPower;
	}
	
	public void setTxPower(double txPower)
	{
		this.txPower = txPower;
	}
	
	
	/*
	 * Finish time
	 */
	public long getFinishTime()
	{
		return this.finishTime;
	}
	
	public void setFinishTime(long time)
	{
		this.finishTime = time;
	}
	
	
	/*
	 * Finish flag
	 */
	public boolean isFinished()
	{
		return this.finished;
	}
	
	public void setFinished(boolean b)
	{
		this.finished = b;
	}
	
	
	/*
	 * Parent node
	 */
	public Node getParent()
	{
		return this.parent;
	}
	
	public void setParent(Node parent)
	{
		this.parent = parent;
	}
	
	
	/*
	 * Cycles
	 */
	public int getCycles()
	{
		return this.cycles;
	}
	
	public void setCycles(int c)
	{
		this.cycles = c;
	}
	
	
	/*
	 * Energy statistics
	 */
	public double getEnergyRecv(FRAME_TYPE ft)
	{
		return this.energy[ft.ordinal()][0];
	}
	
	public double getEnergyRecv()
	{
		double energy = 0;
		
		for(double[] d : this.energy)
			energy += d[0];
		
		return energy;
	}
	
	public double getEnergySent(FRAME_TYPE ft)
	{
		return this.energy[ft.ordinal()][1];
	}
	
	public double getEnergySend()
	{
		double energy = 0;
		
		for(double[] d : this.energy)
			energy += d[1];
		
		return energy;
	}
	
	public void setEnergyRecv(FRAME_TYPE ft, double energy)
	{
		this.energy[ft.ordinal()][0] = energy;
	}
	
	public void setEnergySent(FRAME_TYPE ft, double energy)
	{
		this.energy[ft.ordinal()][1] = energy;
	}
	
	
	/*
	 * Data statistics
	 */
	public int getDataRecv(FRAME_TYPE ft)
	{
		return this.data[ft.ordinal()][0];
	}
	
	public int getPacketsRecv(FRAME_TYPE ft)
	{
		return this.packets[ft.ordinal()][0];
	}
	
	public int getDataSent(FRAME_TYPE ft)
	{
		return this.data[ft.ordinal()][1];
	}
	
	public int getPacketsSent(FRAME_TYPE ft)
	{
		return this.packets[ft.ordinal()][1];
	}
	
	public void setDataRecv(FRAME_TYPE ft, int packets, int data)
	{
		this.data[ft.ordinal()][0] = data;
		this.packets[ft.ordinal()][0] = packets;
	}
	
	public void setDataSent(FRAME_TYPE ft, int packets, int data)
	{
		this.data[ft.ordinal()][1] = data;
		this.packets[ft.ordinal()][1] = packets;
	}
	
	public int getMissingPackets()
	{
		return this.missingPackets;
	}
	
	public void setMissingPackets(int m)
	{
		this.missingPackets = m;
	}
}