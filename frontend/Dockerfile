# Build Stage
FROM node:20-alpine AS build
WORKDIR /app

# Copy dependency manifests
COPY package*.json ./

# Install packages
RUN npm install

# Copy source and configurations
COPY . .

# Compile application to static assets (in dist/)
RUN npm run build

# Run Stage
FROM nginx:alpine

# Copy compiled assets from build stage to Nginx directory
COPY --from=build /app/dist /usr/share/nginx/html

# Replace default Nginx configuration with our custom reverse-proxy configuration
COPY nginx.conf /etc/nginx/conf.d/default.conf

EXPOSE 80

CMD ["nginx", "-g", "daemon off;"]
