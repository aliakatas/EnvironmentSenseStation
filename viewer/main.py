import pandas as pd
import plotly.graph_objects as go
import plotly.express as px
# from plotly.subplots import make_subplots
# import psycopg2
from sqlalchemy import create_engine
import os
# from datetime import datetime, timedelta
from secrets import *

class TimeseriesPlotter:
   def __init__(self, db_config=None):
      """
      Initialize the TimeseriesPlotter with database configuration.
      
      Args:
         db_config (dict): Database configuration dictionary
                           If None, will try to read from environment variables
      """
      if db_config is None:
         self.db_config = {
               'host': os.getenv('DB_HOST', 'localhost'),
               'port': os.getenv('DB_PORT', '5432'),
               'database': os.getenv('DB_NAME', 'your_database'),
               'username': os.getenv('DB_USER', 'your_username'),
               'password': os.getenv('DB_PASSWORD', 'your_password')
         }
      else:
         self.db_config = db_config
      
      self.engine = None
      self.data = None
    
   def connect_to_database(self):
      """Create a connection to the PostgreSQL database."""
      try:
         connection_string = f"postgresql://{self.db_config['username']}:{self.db_config['password']}@{self.db_config['host']}:{self.db_config['port']}/{self.db_config['database']}"
         self.engine = create_engine(connection_string)
         print("Successfully connected to PostgreSQL database")
         return True
      except Exception as e:
         print(f"Error connecting to database: {e}")
         return False
   
   def fetch_timeseries_data(self, query=None, table_name=None, date_column='timestamp', value_columns=None, 
                           start_date=None, end_date=None, limit=None):
      """
      Fetch timeseries data from PostgreSQL database.
      
      Args:
         query (str): Custom SQL query (overrides other parameters if provided)
         table_name (str): Name of the table to query
         date_column (str): Name of the timestamp/date column
         value_columns (list): List of value columns to retrieve
         start_date (str): Start date filter (YYYY-MM-DD format)
         end_date (str): End date filter (YYYY-MM-DD format)
         limit (int): Maximum number of rows to retrieve
      
      Returns:
         pandas.DataFrame: Retrieved timeseries data
      """
      if not self.engine:
         if not self.connect_to_database():
               return None
      
      try:
         if query:
            # Use custom query
            self.data = pd.read_sql_query(query, self.engine)
         else:
            # Build query from parameters
            if not table_name:
               raise ValueError("Either 'query' or 'table_name' must be provided")
            
            if value_columns is None:
               value_columns = ['value']  # Default column name
            
            # Construct SELECT clause
            columns = [date_column] + value_columns
            select_clause = ', '.join(columns)
            
            # Base query
            query = f"SELECT {select_clause} FROM {table_name}"
            
            # Add WHERE conditions
            conditions = []
            if start_date:
               conditions.append(f"{date_column} >= '{start_date}'")
            if end_date:
               conditions.append(f"{date_column} <= '{end_date}'")
            
            if conditions:
               query += " WHERE " + " AND ".join(conditions)
            
            # Add ORDER BY
            query += f" ORDER BY {date_column}"
            
            # Add LIMIT
            if limit:
               query += f" LIMIT {limit}"
            
            print(f"Executing query: {query}")
            self.data = pd.read_sql_query(query, self.engine)
         
         # Convert date column to datetime if it's not already
         if date_column in self.data.columns:
            self.data[date_column] = pd.to_datetime(self.data[date_column])
            self.data = self.data.sort_values(date_column)
         
         print(f"Retrieved {len(self.data)} rows of data")
         print(f"Data shape: {self.data.shape}")
         print(f"Columns: {list(self.data.columns)}")
         
         return self.data
         
      except Exception as e:
         print(f"Error fetching data: {e}")
         return None
   
   def create_interactive_plot(self, date_column='timestamp', value_columns=None, 
                           plot_type='line', title=None, show_points=False,
                           color_palette=None):
      """
      Create an interactive timeseries plot using Plotly.
      
      Args:
         date_column (str): Name of the date/timestamp column
         value_columns (list): List of value columns to plot
         plot_type (str): Type of plot ('line', 'scatter', 'bar')
         title (str): Plot title
         show_points (bool): Whether to show individual data points
         color_palette (list): List of colors for different series
      
      Returns:
         plotly.graph_objects.Figure: Interactive plot figure
      """
      if self.data is None or self.data.empty:
         print("No data available. Please fetch data first.")
         return None
      
      if value_columns is None:
         # Auto-detect numeric columns (excluding the date column)
         numeric_cols = self.data.select_dtypes(include=['number']).columns.tolist()
         if date_column in numeric_cols:
               numeric_cols.remove(date_column)
         value_columns = numeric_cols[:5]  # Limit to first 5 numeric columns
      
      if not value_columns:
         print("No numeric columns found to plot")
         return None
      
      # Create the plot
      fig = go.Figure()
      
      # Set default color palette
      if color_palette is None:
         color_palette = px.colors.qualitative.Set1
      
      # Add traces for each value column
      for i, col in enumerate(value_columns):
         if col not in self.data.columns:
            print(f"Warning: Column '{col}' not found in data")
            continue
         
         color = color_palette[i % len(color_palette)]
         
         if plot_type == 'line':
            mode = 'lines+markers' if show_points else 'lines'
            fig.add_trace(go.Scatter(
               x=self.data[date_column],
               y=self.data[col],
               mode=mode,
               name=col,
               line=dict(color=color),
               hovertemplate=f'<b>{col}</b><br>' +
                           f'{date_column}: %{{x}}<br>' +
                           f'Value: %{{y:,.2f}}<extra></extra>'
            ))
         elif plot_type == 'scatter':
            fig.add_trace(go.Scatter(
               x=self.data[date_column],
               y=self.data[col],
               mode='markers',
               name=col,
               marker=dict(color=color),
               hovertemplate=f'<b>{col}</b><br>' +
                           f'{date_column}: %{{x}}<br>' +
                           f'Value: %{{y:,.2f}}<extra></extra>'
            ))
         elif plot_type == 'bar':
            fig.add_trace(go.Bar(
               x=self.data[date_column],
               y=self.data[col],
               name=col,
               marker=dict(color=color),
               hovertemplate=f'<b>{col}</b><br>' +
                           f'{date_column}: %{{x}}<br>' +
                           f'Value: %{{y:,.2f}}<extra></extra>'
            ))
      
      # Update layout
      if title is None:
         title = f"Timeseries Plot - {', '.join(value_columns)}"
      
      fig.update_layout(
         title={
               'text': title,
               'x': 0.5,
               'xanchor': 'center',
               'font': {'size': 20}
         },
         xaxis_title=date_column.capitalize(),
         yaxis_title="Value",
         hovermode='x unified',
         showlegend=True,
         width=1200,
         height=600,
         template='plotly_white'
      )
      
      # Add range selector
      fig.update_layout(
         xaxis=dict(
               rangeselector=dict(
                  buttons=list([
                     dict(count=7, label="7D", step="day", stepmode="backward"),
                     dict(count=30, label="30D", step="day", stepmode="backward"),
                     dict(count=90, label="3M", step="day", stepmode="backward"),
                     dict(count=180, label="6M", step="day", stepmode="backward"),
                     dict(count=365, label="1Y", step="day", stepmode="backward"),
                     dict(step="all", label="All")
                  ])
               ),
               rangeslider=dict(visible=True),
               type="date"
         )
      )
      
      return fig
   
   def show_plot(self, **kwargs):
      """Create and display the interactive plot."""
      fig = self.create_interactive_plot(**kwargs)
      if fig:
         fig.show()
      return fig
   
   def save_plot(self, filename="timeseries_plot.html", **kwargs):
      """Create and save the interactive plot to HTML file."""
      fig = self.create_interactive_plot(**kwargs)
      if fig:
         fig.write_html(filename)
         print(f"Plot saved to {filename}")
      return fig


def main():
   """Example usage of the TimeseriesPlotter class."""
   
   # Example 1: Using environment variables for database connection
   print("=== Example 1: Basic Usage ===")
   plotter = TimeseriesPlotter()
   
   # You can also specify database configuration directly:
   # db_config = {
   #     'host': 'localhost',
   #     'port': '5432',
   #     'database': 'your_database',
   #     'username': 'your_username',
   #     'password': 'your_password'
   # }
   # plotter = TimeseriesPlotter(db_config)
   
   # Example query - adjust according to your table structure
   sample_query = """
      SELECT 
         created_at as timestamp,
         price,
         volume,
         market_cap
      FROM stock_prices 
      WHERE created_at >= '2024-01-01' 
      ORDER BY created_at
      LIMIT 1000
   """
   
   # Fetch data using custom query
   data = plotter.fetch_timeseries_data(query=sample_query)
   
   if data is not None:
      # Create and show interactive plot
      fig = plotter.show_plot(
         date_column='timestamp',
         value_columns=['price', 'volume'],
         title="Stock Price and Volume Over Time",
         show_points=False
      )
      
      # Save plot to file
      plotter.save_plot("stock_timeseries.html")
   
   # Example 2: Using table-based approach
   print("\n=== Example 2: Table-based Query ===")
   
   # Fetch data by specifying table and columns
   data2 = plotter.fetch_timeseries_data(
      table_name='metrics',
      date_column='measurement_time',
      value_columns=['cpu_usage', 'memory_usage', 'disk_usage'],
      start_date='2024-01-01',
      end_date='2024-12-31',
      limit=5000
   )
   
   if data2 is not None:
      # Create multiple plot types
      fig_line = plotter.create_interactive_plot(
         date_column='measurement_time',
         value_columns=['cpu_usage', 'memory_usage'],
         plot_type='line',
         title="System Metrics - Line Plot"
      )
      
      fig_scatter = plotter.create_interactive_plot(
         date_column='measurement_time',
         value_columns=['disk_usage'],
         plot_type='scatter',
         title="Disk Usage - Scatter Plot",
         show_points=True
      )
      
      if fig_line:
         fig_line.show()
      if fig_scatter:
         fig_scatter.show()


if __name__ == "__main__":
   # Set environment variables (you should set these in your environment)
   #  os.environ['DB_HOST'] = 'localhost'
   #  os.environ['DB_PORT'] = '5432'
   #  os.environ['DB_NAME'] = 'your_database'
   #  os.environ['DB_USER'] = 'your_username'
   #  os.environ['DB_PASSWORD'] = 'your_password'

   # main()

   os.environ['DB_HOST'] = HOST
   os.environ['DB_PORT'] = str(PORT)
   os.environ['DB_NAME'] = DATABASE
   os.environ['DB_USER'] = DBUSER
   os.environ['DB_PASSWORD'] = DBUSERPASS
   
   print("=== Example 1: Basic Usage ===")
   plotter = TimeseriesPlotter()
   
   # Example query - adjust according to your table structure
   sample_query = f"""
      SELECT 
         date_time as timestamp,
         temperature,
         humidity,
         pressure,
         soil_moisture,
         board_temperature
      FROM {TABLENAME} 
      ORDER BY timestamp
      LIMIT 1000
   """
   
   # Fetch data using custom query
   data = plotter.fetch_timeseries_data(query=sample_query)
   
   if data is not None:
      # Create and show interactive plot
      fig = plotter.show_plot(
         date_column='timestamp',
         value_columns=['temperature', 'humidity', 'pressure', 'soil_moisture'],
         title="Saloon environment",
         show_points=False
      )
      
      # Save plot to file
      plotter.save_plot("saloon_environment.html")

   # Example 2: Using table-based approach
   print("\n=== Example 2: Table-based Query ===")
   
   # Fetch data by specifying table and columns
   data2 = plotter.fetch_timeseries_data(
      table_name=TABLENAME,
      date_column='date_time',
      value_columns=['temperature', 'humidity', 'pressure', 'soil_moisture', 'board_temperature'],
      limit=5000
   )
   
   if data2 is not None:
      # Create multiple plot types
      fig_line = plotter.create_interactive_plot(
         date_column='date_time',
         value_columns=['temperature', 'humidity', 'pressure', 'soil_moisture'],
         plot_type='line',
         title="Environment Metrics - Line Plot"
      )
      
      fig_scatter = plotter.create_interactive_plot(
         date_column='date_time',
         value_columns=['board_temperature', 'temperature'],
         plot_type='scatter',
         title="Temperatures - Scatter Plot",
         show_points=True
      )
      
      if fig_line:
         fig_line.show()
      if fig_scatter:
         fig_scatter.show()
   
