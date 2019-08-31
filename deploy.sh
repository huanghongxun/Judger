kubectl config set-cluster "$CI_PROJECT_NAME" --server="$KUBE_URL" --certificate-authority="$KUBE_CA_PEM_FILE"
kubectl config set-credentials "$CI_PROJECT_NAME" --token="$KUBE_TOKEN"
kubectl config set-context "$CI_PROJECT_NAME" --namespace="$KUBE_NAMESPACE" --cluster="$CI_PROJECT_NAME" --user="$CI_PROJECT_NAME"
kubectl config use-context "$CI_PROJECT_NAME"
sed -i "s~<IMAGE_NAME>~${DOCKER_IMAGE_NAME}~g" k8s-deploy.yaml
sed -i "s~<DEPLOY_NAME>~${DEPLOY_NAME}~g" k8s-deploy.yaml
kubectl create namespace "$KUBE_NAMESPACE" --save-config --dry-run -o yaml | kubectl apply -f -
kubectl create secret docker-registry gitlab-registry --docker-server=$CI_REGISTRY --docker-username=$DEPLOY_USER --docker-password=$DEPLOY_PASSWORD --docker-email=$DEPLOY_EMAIL --save-config --dry-run -o yaml | kubectl apply -f -
kubectl patch serviceaccount default -p '{"imagePullSecrets":[{"name":"gitlab-registry"}]}'
kubectl apply -f k8s-deploy.yaml

