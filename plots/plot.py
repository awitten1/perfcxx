
import duckdb
import sys
import plotly.express as px

csv_file = sys.argv[1]

def query_data():
    conn = duckdb.connect()
    return conn.execute(f"""
        select
            l1d_cache_accesses, vector_size
        from '{csv_file}'
        where access_pattern = 'random'
        order by vector_size asc
    """
    ).df()


def generate_plot(df):
    # Plot with Plotly Express
    fig = px.line(
        df,
        x="vector_size",
        y="l1d_cache_accesses",
        title="L1D Cache Accesses vs Vector Size",
        labels={"vector_size": "Vector Size", "l1d_cache_accesses": "L1D Cache Accesses"},
        markers=True
    )

    # Save plot as PNG using Kaleido
    fig.write_image("plot.png")
    print("Plot saved to plot.png")

    # Show interactive plot in browser
    fig.show()


def main():
    df = query_data()
    generate_plot(df)

if __name__ == "__main__":
    main()